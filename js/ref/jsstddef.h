/*stddef inclusion here to first declare ptrdif as a signed long instead of a signed int*/

#ifdef _WINDOWS
# ifndef XP_WIN
# define XP_WIN
# endif
#if defined(_WIN32) || defined(WIN32)
# ifndef XP_WIN32
# define XP_WIN32
# endif
#else
# ifndef XP_WIN16
# define XP_WIN16
# endif
#endif
#endif

#ifdef XP_WIN16
#ifndef _PTRDIFF_T_DEFINED
typedef long ptrdiff_t;

/*
 * The Win16 compiler treats pointer differences as 16-bit signed values.
 * This macro allows us to treat them as 17-bit signed values, stored in
 * a 32-bit type.
 */
#define PTRDIFF(p1, p2, type)                                 \
    ((((unsigned long)(p1)) - ((unsigned long)(p2))) / sizeof(type))

#define _PTRDIFF_T_DEFINED
#endif /*_PTRDIFF_T_DEFINED*/
#else /*WIN16*/

#define PTRDIFF(p1, p2, type)                                 \
	((p1) - (p2))
	
#endif

#include <stddef.h>


