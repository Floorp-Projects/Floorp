/*
 * @(#)x86.uslc
 *
 * UnixWare has threads in libthread, but OpenServer doesn't (yet).
 *
 * For cc/x86, 0 is clear, 1 is set.
 */

#if defined(__USLC__)
asm int
_tsl_set(void *tsl)
{
%mem tsl
	movl	tsl, %ecx
	movl	$1, %eax
	lock
	xchgb	(%ecx),%al
	xorl	$1,%eax
}
#endif

#define	TSL_SET(tsl)	_tsl_set(tsl)
#define	TSL_UNSET(tsl)	(*(tsl) = 0)
#define	TSL_INIT(tsl)	TSL_UNSET(tsl)
