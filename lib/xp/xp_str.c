/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


#include "xp.h"
#include "xp_str.h"
#include <ctype.h>
#include <stdarg.h>

#ifdef PROFILE
#pragma profile on
#endif

#if !defined(XP_WIN) && !defined(XP_OS2) /* keep this out of the winfe */
#if defined(DEBUG)

char *
NOT_NULL (const char * p)
{
	XP_ASSERT(p);
	return (char*) p;
}

#endif
#endif

#define TOLOWER_TABLE_SIZE 128

PRIVATE uint8 *
xp_get_tolower_table()
{
	static uint8 tolower_table[TOLOWER_TABLE_SIZE];
    static XP_Bool first_time = TRUE;

    if(first_time)
    {
        int i;
        first_time = FALSE;

        /* build the tolower_table */
        for(i=0; i < sizeof(tolower_table); i++)
        {
            tolower_table[i] = (uint8) tolower((char)i);
        }
    }

	return tolower_table;
}

PRIVATE uint8 *
xp_get_toupper_table()
{
	static uint8 toupper_table[TOLOWER_TABLE_SIZE];
    static XP_Bool first_time = TRUE;

    if(first_time)
    {
        int i;
        first_time = FALSE;

        /* build the tolower_table */
        for(i=0; i < sizeof(toupper_table); i++)
        {
            toupper_table[i] = (uint8) toupper((char)i);
        }
    }

	return toupper_table;
}

#define XP_STR_TOUPPER(c) (c < TOLOWER_TABLE_SIZE ? toupper_table[c] : c)
#define XP_STR_TOLOWER(c) (c < TOLOWER_TABLE_SIZE ? tolower_table[c] : c)

/* fast table driven tolower routine 
 *
 * We only deal with 7 bit ascii
 */
PUBLIC int
xp_tolower(int c)
{
	uint8 *tolower_table = xp_get_tolower_table();

	return XP_STR_TOLOWER(c);
}

/* fast table driven toupper routine 
 *
 * We only deal with 7 bit ascii
 */
PUBLIC int
xp_toupper(int c)
{
	uint8 *toupper_table = xp_get_toupper_table();

	return XP_STR_TOUPPER(c);
}

/* find a substring within a string with a case insensitive search
 */
PUBLIC char *
strcasestr (const char * str, const char * substr)
{
    register const char *pA;
    register const char *pB;
    register const char *pC;
	uint8 *toupper_table = xp_get_toupper_table();

	if(!str)
		return(NULL);
	
	for(pA=str; *pA; pA++)
	  {
		if(XP_STR_TOUPPER(*pA) == XP_STR_TOUPPER(*substr))
		  {
    		for(pB=pA, pC=substr; ; pB++, pC++)
              {
                if(!(*pC))
                    return((char *)pA);

				if(!(*pB))
					break;

				if(XP_STR_TOUPPER(*pB) != XP_STR_TOUPPER(*pC))
					break;
              }
		  }
	  }

	return(NULL);
}

/* find a char within a specified length string
 */
PUBLIC char *
strnchr (const char * str, const char single, int32 len)
{
	register int count=0;
    register const char *pA;

    if(!str)
        return(NULL);
   
    for(pA=str; count < len; pA++, count++)
      {
        if(*pA == single)
          {
             return((char *)pA);
          }

		if(!(*pA))
			return(NULL);
      }

    return(NULL);
}

/* find a substring within a specified length string with a case 
 * insensitive search
 */
PUBLIC char *
strncasestr (const char * str, const char * substr, int32 len)
{
	register int count=0;
	register int count2=0;
    register const char *pA;
    register const char *pB;
    register const char *pC;
	uint8 *toupper_table = xp_get_toupper_table();

    if(!str || !substr)
        return(NULL);
   
    for(pA=str; count < len; pA++, count++)
      {
        if(XP_STR_TOUPPER(*pA) == XP_STR_TOUPPER(*substr))
          {
            for(pB=pA, pC=substr, count2=count; count2<len; 
													count2++,pB++,pC++)
              {
                if(!(*pC))
                    return((char *)pA);

                if(!(*pB))
                    break;

                if(XP_TO_UPPER(*pB) != XP_TO_UPPER(*pC))
                    break;
              }
          }
      }

    return(NULL);
}


/*	compare strings in a case insensitive manner
*/
PUBLIC int 
strcasecomp (const char* one, const char *two)
{
	const char *pA;
	const char *pB;
	uint8 *toupper_table = xp_get_toupper_table();

	for(pA=one, pB=two; *pA && *pB; pA++, pB++) 
	  {
	    register int tmp = XP_STR_TOUPPER(*pA) - XP_STR_TOUPPER(*pB);
	    if (tmp) 
			return tmp;
	  }

	if (*pA) 
		return 1;	
	if (*pB) 
		return -1;
	return 0;	
}


/*	compare strings in a case insensitive manner with a length limit
*/
PUBLIC int 
strncasecomp (const char* one, const char * two, int n)
{
	const char *pA;
	const char *pB;
	uint8 *toupper_table = xp_get_toupper_table();
	
	for(pA=one, pB=two;; pA++, pB++) 
	  {
	    int tmp;
	    if (pA == one+n) 
			return 0;	
	    if (!(*pA && *pB)) 
			return *pA - *pB;
	    tmp = XP_STR_TOUPPER(*pA) - XP_STR_TOUPPER(*pB);
	    if (tmp) 
			return tmp;
	  }
}

/************************************************************************
 * These are "safe" versions of the runtime library routines. The RTL
 * versions do not null-terminate dest IFF strlen(src) >= destLength.
 * These versions always null-terminate, which is why they're safe.
 */

char *XP_STRNCAT_SAFE (char *dest, const char *src, size_t maxToCat)
{
	int destLen; 
	char *result; 

	destLen = XP_STRLEN (dest);
	result = strncat (dest, src, --maxToCat);
	dest[destLen + maxToCat] = '\0';
	return result;
}

char *XP_STRNCPY_SAFE (char *dest, const char *src, size_t destLength)
{
	char *result = strncpy (dest, src, --destLength);
	dest[destLength] = '\0';
	return result;
}

/*************************************************
   The following functions are used to implement
   a thread safe strtok
 *************************************************/
/*
 * Get next token from string *stringp, where tokens are (possibly empty)
 * strings separated by characters from delim.  Tokens are separated
 * by exactly one delimiter iff the skip parameter is false; otherwise
 * they are separated by runs of characters from delim, because we
 * skip over any initial `delim' characters.
 *
 * Writes NULs into the string at *stringp to end tokens.
 * delim will usually, but need not, remain CONSTant from call to call.
 * On return, *stringp points past the last NUL written (if there might
 * be further tokens), or is NULL (if there are definitely no more tokens).
 *
 * If *stringp is NULL, strtoken returns NULL.
 */
static 
char *strtoken_r(char ** stringp, const char *delim, int skip)
{
	char *s;
	const char *spanp;
	int c, sc;
	char *tok;

	if ((s = *stringp) == NULL)
		return (NULL);

	if (skip) {
		/*
		 * Skip (span) leading delimiters (s += strspn(s, delim)).
		 */
	cont:
		c = *s;
		for (spanp = delim; (sc = *spanp++) != 0;) {
			if (c == sc) {
				s++;
				goto cont;
			}
		}
		if (c == 0) {		/* no token found */
			*stringp = NULL;
			return (NULL);
		}
	}

	/*
	 * Scan token (scan for delimiters: s += strcspn(s, delim), sort of).
	 * Note that delim must have one NUL; we stop if we see that, too.
	 */
	for (tok = s;;) {
		c = *s++;
		spanp = delim;
		do {
			if ((sc = *spanp++) == c) {
				if (c == 0)
					s = NULL;
				else
					s[-1] = 0;
				*stringp = s;
				return( (char *) tok );
			}
		} while (sc != 0);
	}
	/* NOTREACHED */
}


char *XP_STRTOK_R(char *s1, const char *s2, char **lasts)
{
	if (s1)
		*lasts = s1;
	return (strtoken_r(lasts, s2, 1));
}


#ifdef PROFILE
#pragma profile off
#endif
