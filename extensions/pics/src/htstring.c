
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
**	@(#) $Id: htstring.c,v 1.1 1999/03/18 22:32:50 neeti%netscape.com Exp $
**
**	Original version came with listserv implementation.
**	Version TBL Oct 91 replaces one which modified the strings.
**	02-Dec-91 (JFG) Added stralloccopy and stralloccat
**	23 Jan 92 (TBL) Changed strallocc* to 8 char HTSAC* for VM and suchlike
**	 6 Oct 92 (TBL) Moved WWW_TraceFlag in here to be in library
**       9 Oct 95 (KR)  fixed problem with double quotes in HTNextField
*/

/* Library include files */
#include <stdarg.h>
#include "plstr.h"
/* #include "sysdep.h"  jhines -- 7/9/97 */
#include "htutils.h"
#include "htstring.h"					 /* Implemented here */

#if WWWTRACE_MODE == WWWTRACE_FILE
PUBLIC FILE *WWWTrace = NULL;
#endif

#ifndef WWW_WIN_DLL
PUBLIC int WWW_TraceFlag = 0;		/* Global trace flag for ALL W3 code */
#endif


/*	Allocate a new copy of a string, and returns it
*/
PUBLIC char * HTSACopy (char ** dest, const char * src)
{
  if (*dest) HT_FREE(*dest);
  if (! src)
    *dest = NULL;
  else {
    if ((*dest  = (char  *) HT_MALLOC(PL_strlen(src) + 1)) == NULL)
        HT_OUTOFMEM("HTSACopy");
    PL_strcpy (*dest, src);
}
  return *dest;
}

/*	String Allocate and Concatenate
*/
PUBLIC char * HTSACat (char ** dest, const char * src)
{
  if (src && *src) {
    if (*dest) {
      int length = PL_strlen (*dest);
      if ((*dest  = (char  *) HT_REALLOC(*dest, length + PL_strlen(src) + 1)) == NULL)
          HT_OUTOFMEM("HTSACat");
      PL_strcpy (*dest + length, src);
    } else {
      if ((*dest  = (char  *) HT_MALLOC(PL_strlen(src) + 1)) == NULL)
          HT_OUTOFMEM("HTSACat");
      PL_strcpy (*dest, src);
    }
  }
  return *dest;
}


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
    while (*tmpl && *name && TOUPPER(*tmpl)==TOUPPER(*name)) tmpl++, name++;
    return ((!*tmpl && !*name) || *tmpl=='*') ? (char *) name : (char *) NULL;
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
	for(p=s;*p;p++);		/* Find end of string */
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

