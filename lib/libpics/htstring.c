
/*  W3 Copyright statement 
Copyright 1995 by: Massachusetts Institute of Technology (MIT), INRIA</H2>

This W3C software is being provided by the copyright holders under the
following license. By obtaining, using and/or copying this software,
you agree that you have read, understood, and will comply with the
following terms and conditions: 

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee or royalty is hereby
granted, provided that the full text of this NOTICE appears on
<EM>ALL</EM> copies of the software and documentation or portions
thereof, including modifications, that you make. 

<B>THIS SOFTWARE IS PROVIDED "AS IS," AND COPYRIGHT HOLDERS MAKE NO
REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED.  BY WAY OF EXAMPLE,
BUT NOT LIMITATION, COPYRIGHT HOLDERS MAKE NO REPRESENTATIONS OR
WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR
THAT THE USE OF THE SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY
THIRD PARTY PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.
COPYRIGHT HOLDERS WILL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE
OR DOCUMENTATION.

The name and trademarks of copyright holders may NOT be used
in advertising or publicity pertaining to the software without
specific, written prior permission.  Title to copyright in this
software and any associated documentation will at all times remain
with copyright holders. 
*/
/*								     HTString.c
**	DYNAMIC STRING UTILITIES
**
**	(c) COPYRIGHT MIT 1995.
**	Please first read the full copyright statement in the file COPYRIGH.
**	@(#) $Id: htstring.c,v 3.2 1998/06/22 21:20:29 spider Exp $
**
**	Original version came with listserv implementation.
**	Version TBL Oct 91 replaces one which modified the strings.
**	02-Dec-91 (JFG) Added stralloccopy and stralloccat
**	23 Jan 92 (TBL) Changed strallocc* to 8 char HTSAC* for VM and suchlike
**	 6 Oct 92 (TBL) Moved WWW_TraceFlag in here to be in library
**       9 Oct 95 (KR)  fixed problem with double quotes in HTNextField
*/

/* Library include files */
/* --- BEGIN added by mharmsen@netscape.com on 7/9/97 --- */
#include "xp.h"
/* --- END added by mharmsen@netscape.com on 7/9/97 --- */
/* #include "sysdep.h"  jhines -- 7/9/97 */
#include "htutils.h"
#include "htstring.h"					 /* Implemented here */

#if WWWTRACE_MODE == WWWTRACE_FILE
PUBLIC FILE *WWWTrace = NULL;
#endif

#ifndef WWW_WIN_DLL
PUBLIC int WWW_TraceFlag = 0;		/* Global trace flag for ALL W3 code */
#endif

/* ------------------------------------------------------------------------- */

/*	Strings of any length
**	---------------------
*/
/* --- BEGIN removed by mharmsen@netscape.com on 7/9/97 --- */
/************************************************************************/
/* PUBLIC int strcasecomp (const char * a, const char * b)              */
/* {                                                                    */
/*     int diff;                                                        */
/*     for( ; *a && *b; a++, b++) {                                     */
/*	if ((diff = TOLOWER(*a) - TOLOWER(*b)))                         */
/*	    return diff;                                                */
/*    }                                                                 */
/*    if (*a) return 1;			*/ /* a was longer than b          */
/*    if (*b) return -1;		*/	/* a was shorter than b */
/*    return 0;	 			    */ /* Exact match                  */
/*}                                                                     */
/************************************************************************/
/* --- END removed by mharmsen@netscape.com on 7/9/97 --- */


/*	With count limit
**	----------------
*/
/* --- BEGIN removed by mharmsen@netscape.com on 7/9/97 --- */
/**********************************************************************/
/* PUBLIC int strncasecomp (const char * a, const char * b, int n)    */
/* {                                                                  */
/*	const char *p =a;                                             */
/*	const char *q =b;                                             */
/*	                                                              */
/*	for(p=a, q=b;; p++, q++) {                                    */
/*	    int diff;                                                 */
/*	    if (p == a+n) return 0;	*/  /*   Match up to n characters */
/*	    if (!(*p && *q)) return *p - *q;                          */
/*	    diff = TOLOWER(*p) - TOLOWER(*q);                         */
/*	    if (diff) return diff;                                    */
/*	}                                                             */
/*	NOTREACHED                                                  */
/*}                                                                   */
/**********************************************************************/
/* --- END removed by mharmsen@netscape.com on 7/9/97 --- */


/*
** strcasestr(s1,s2) -- like strstr(s1,s2) but case-insensitive.
*/
/* --- BEGIN removed by mharmsen@netscape.com on 7/9/97 --- */
/*************************************************************************/
/* PUBLIC char * strcasestr (char * s1, char * s2)                       */
/* {                                                                     */
/*    char * ptr = s1;                                                   */
/*                                                                       */
/*    if (!s1 || !s2 || !*s2) return s1;                                 */
/*                                                                       */
/*    while (*ptr) {                                                     */
/*	if (TOUPPER(*ptr) == TOUPPER(*s2)) {                             */
/*	    char * cur1 = ptr + 1;                                       */
/*	    char * cur2 = s2 + 1;                                        */
/*	    while (*cur1 && *cur2 && TOUPPER(*cur1) == TOUPPER(*cur2)) { */
/*		cur1++;                                                  */
/*		cur2++;                                                  */
/*	    }                                                            */
/*	    if (!*cur2)	return ptr;                                      */
/*	}                                                                */
/*	ptr++;                                                           */
/*    }                                                                  */
/*    return NULL;                                                       */
/*}                                                                      */
/*************************************************************************/
/* --- END removed by mharmsen@netscape.com on 7/9/97 --- */


#if 0 /* LJM: not needed */

/*	Allocate a new copy of a string, and returns it
*/
PUBLIC char * HTSACopy (char ** dest, const char * src)
{
  if (*dest) HT_FREE(*dest);
  if (! src)
    *dest = NULL;
  else {
    /* --- BEGIN converted by mharmsen@netscape.com on 7/9/97 --- */
    if ((*dest  = (char  *) HT_MALLOC(XP_STRLEN(src) + 1)) == NULL)
        HT_OUTOFMEM("HTSACopy");
    XP_STRCPY (*dest, src);
    /* --- END converted by mharmsen@netscape.com on 7/9/97 --- */
  }
  return *dest;
}

/*	String Allocate and Concatenate
*/
PUBLIC char * HTSACat (char ** dest, const char * src)
{
  if (src && *src) {
    /* --- BEGIN converted by mharmsen@netscape.com on 7/9/97 --- */
    if (*dest) {
      int length = XP_STRLEN (*dest);
      if ((*dest  = (char  *) HT_REALLOC(*dest, length + XP_STRLEN(src) + 1)) == NULL)
          HT_OUTOFMEM("HTSACat");
      XP_STRCPY (*dest + length, src);
    } else {
      if ((*dest  = (char  *) HT_MALLOC(XP_STRLEN(src) + 1)) == NULL)
          HT_OUTOFMEM("HTSACat");
      XP_STRCPY (*dest, src);
    }
    /* --- END converted by mharmsen@netscape.com on 7/9/97 --- */
  }
  return *dest;
}

#endif /* 0 */

/*	String Matching
**	---------------
**	String comparison function for file names with one wildcard * in the
**	template. Arguments are:
**
**	tmpl	is a template string to match the name against.
**		agaist, may contain a single wildcard character * which
**		matches zero or more arbitrary characters.
**	name	is the name to be matched agaist the template.
**
**	return:	- Empty string if perfect match
**		- pointer to part matched by wildcard if any
**		- NULL if no match
*/
PUBLIC char * HTStrMatch (const char * tmpl, const char * name)
{
    while (*tmpl && *name && *tmpl==*name) tmpl++, name++;
    return ((!*tmpl && !*name) || *tmpl=='*') ? (char *) name : (char *) NULL;
}    

PUBLIC char * HTStrCaseMatch (const char * tmpl, const char * name)
{
    /* --- BEGIN converted by mharmsen@netscape.com on 7/9/97 --- */
    while (*tmpl && *name && XP_TO_UPPER(*tmpl)==XP_TO_UPPER(*name)) tmpl++, name++;
    return ((!*tmpl && !*name) || *tmpl=='*') ? (char *) name : (char *) NULL;
    /* --- END converted by mharmsen@netscape.com on 7/9/97 --- */
}    

/*	Strip white space off a string
**	------------------------------
**	Return value points to first non-white character, or to 0 if none.
**	All trailing white space is OVERWRITTEN with zero.
*/
PUBLIC char * HTStrip (char * s)
{
    if (s) {
	char * p=s;
	for(p=s;*p;p++) {;}	/* Find end of string */
	for(p--;p>=s;p--) {
	    if (WHITE(*p))
		*p=0;			/* Zap trailing blanks */
	    else
		break;
	}
	while (WHITE(*s)) s++;		/* Strip leading blanks */
    }
    return s;
}

PRIVATE HTTraceCallback * PHTTraceCallback;

PUBLIC void HTTrace_setCallback(HTTraceCallback * pCall)
{
    PHTTraceCallback = pCall;
}

PUBLIC HTTraceCallback * HTTrace_getCallback(void)
{
    return PHTTraceCallback;
}

PUBLIC int HTTrace(const char * fmt, ...)
{
    va_list pArgs;
    va_start(pArgs, fmt);
    if (PHTTraceCallback)
	(*PHTTraceCallback)(fmt, pArgs);
#ifdef WWW_WIN_WINDOWS
    return (0);
#else
    return (vfprintf(stderr, fmt, pArgs));
#endif
}

