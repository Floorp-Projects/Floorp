
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
/*                                     W3C Reference Library libwww Generic String Management
                                GENERIC STRING MANAGEMENT
                                             
 */
/*
**      (c) COPYRIGHT MIT 1995.
**      Please first read the full copyright statement in the file COPYRIGH.
*/
/*

   These functions provide functionality for case-independent string comparison and
   allocations with copies etc.
   
   This module is implemented by HTString.c, and it is a part of the W3C Reference
   Library.
   
 */
#ifndef HTSTRING_H
#define HTSTRING_H

#include "xp_core.h"

PR_BEGIN_EXTERN_C

/*


DYNAMIC STRING MANIPULATION

   These two functions are dynamic versions of strcpyand strcat. They use mallocfor
   allocating space for the string. If StrAllocCopyis called with a non-NULL dest, then
   this is freed before the new value is assigned so that only the laststring created has
   to be freed by the user. If StrAllocCatis called with a NULL pointer as destination
   then it is equivalent to StrAllocCopy.
   
 */
 #define StrAllocCopy(dest, src) HTSACopy (&(dest), src) 
 #define StrAllocCat(dest, src)  HTSACat  (&(dest), src)  

extern char * HTSACopy (char **dest, const char *src);
extern char * HTSACat  (char **dest, const char *src);
/*

CASE-INSENSITIVE STRING COMPARISON

   The usual routines (comp instead of cmp) had some problem.
   
 */
/* extern int strcasecomp  (const char *a, const char *b); */
/* extern int strncasecomp (const char *a, const char *b, int n); */
/*

STRING COMPARISON WITH WILD CARD MATCH

   String comparison function for file names with one wildcard * in the template.
   Arguments are:
   
  tmpl                   is a template string to match the name against. agaist, may
                         contain a single wildcard character * which matches zero or more
                         arbitrary characters.
                         
  name                   is the name to be matched agaist the template.
                         
   Returns empty string ("") if perfect match, pointer to part matched by wildcard if any,
   or NULL if no match. This is basically the same as YES if match, else NO.
   
 */
extern char * HTStrMatch        (const char * tmpl, const char * name);
extern char * HTStrCaseMatch    (const char * tmpl, const char * name);
/*

CASE-INSENSITIVE STRSTR

   This works like strstr()but is not case-sensitive.
   
 */
/* extern char * strcasestr (char * s1, char * s2); */
/*

STRIP WHITE SPACE OFF A STRING

   Return value points to first non-white character, or to '/0' if none. All trailing
   white space is OVERWRITTEN with zero.
   
 */
extern char * HTStrip (char * s);
/*

 */

PR_END_EXTERN_C

#endif
/*

   
   ___________________________________
   
                          @(#) $Id: htstring.h,v 1.1 1999/03/18 22:32:50 neeti%netscape.com Exp $
                                                                                          
    */
