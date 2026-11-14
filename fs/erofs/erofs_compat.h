/* SPDX-License-Identifier: GPL-2.0 */
/*
 * 4.14 兼容层
 *
 * 来源驱动（OPPO/Realme MT6885, 4.14.186）的 internal.h 用了
 * atomic_cond_read_relaxed()，该宏在 4.20 才进主线，
 * cezanne 官方 4.14.186 树里没有。这里给出语义等价实现：
 * 自旋等待直到值不再等于 locked，返回观察到的值。
 */
#ifndef __EROFS_COMPAT_414_H
#define __EROFS_COMPAT_414_H

#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/compiler.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <asm/processor.h>

/*
 * EROFS_SUPER_MAGIC_V1 在主线里定义于 include/uapi/linux/magic.h，
 * 但 cezanne 官方 4.14.186 树的 magic.h 里还没有（EROFS 是 4.19 才进主线的）。
 * 数值取自主线 uapi/linux/magic.h，与 OS4 镜像里的 superblock magic 一致。
 */
#ifndef EROFS_SUPER_MAGIC_V1
#define EROFS_SUPER_MAGIC_V1	0xE0F5E1E2
#endif

/*
 * attach_page_private / detach_page_private 是 5.8 才加入的页面私有数据
 * 辅助函数（主线 include/linux/pagemap.h），4.14 没有。
 *
 * 语义必须严格照 5.8 的实现来，否则页引用计数会错：
 *   attach: get_page() 额外持有一个引用，再设置 private 数据并置位 PagePrivate
 *   detach: 取出数据、清私有序、清 PagePrivate，并 put_page() 释放那个引用
 *
 * zdata.c 里的用法印证了这一点：
 *   add_to_page_cache_lru(page, ...) 拿 1 个 ref
 *   attach_page_private(page, pcl)   → 再 +1
 *   put_page(page)                   → 减掉 allocpage 的 ref，剩 2 个
 *   注释原文："drop a refcount added by allocpage (then we have 2 refs here)"
 */
#ifndef attach_page_private
#define attach_page_private(page, data) do {				\
	get_page(page);							\
	set_page_private((page), (unsigned long)(data));		\
	SetPagePrivate(page);						\
} while (0)
#endif

#ifndef detach_page_private
static inline void *__erofs_detach_page_private(struct page *page)
{
	void *data = (void *)page_private(page);

	if (!data)
		return NULL;
	set_page_private(page, 0);
	ClearPagePrivate(page);
	put_page(page);
	return data;
}
#define detach_page_private(page) __erofs_detach_page_private(page)
#endif

static inline int erofs_compat_cond_read_relaxed(atomic_t *v, int locked)
{
	int val;

	for (;;) {
		val = atomic_read(v);
		if (val != locked)
			break;
		cpu_relax();
	}
	smp_mb();
	return val;
}

#endif /* __EROFS_COMPAT_414_H */
