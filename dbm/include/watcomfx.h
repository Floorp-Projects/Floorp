#if defined(__WATCOMC__) || defined(__WATCOM_CPLUSPLUS__)
#ifndef __WATCOM_FIX_H__
#define __WATCOM_FIX_H__ 1
/*
 * WATCOM's C compiler doesn't default to "__cdecl" conventions for external
 * symbols and functions.  Rather than adding an explicit __cdecl modifier to 
 * every external symbol and function declaration and definition, we use the 
 * following pragma to (attempt to) change WATCOM c's default to __cdecl.
 * These pragmas were taken from pages 180-181, 266 & 269 of the 
 * Watcom C/C++ version 11 User's Guide, 3rd edition.
 */
#if defined(XP_WIN16) || defined(WIN16) 
#pragma aux default "_*" \
	parm caller [] \
	value struct float struct routine [ax] \
	modify [ax bx cx dx es]
#else
#pragma aux default "_*" \
	parm caller [] \
	value struct float struct routine [eax] \
	modify [eax ecx edx]
#endif
#pragma aux default far

#endif /* once */
#endif /* WATCOM compiler */
