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
/*	metatag.c	*/

#include "intlpriv.h"
#include "xp.h"
#include "libi18n.h"


/* 
  This function return csid if it finds Meta charset info

  Currently there are two way to specify Meta charset
    1. <META HTTP-EQUIV=Content-Type CONTENT="text/html; charset=iso-8859-1">
    2. <META charset=iso-8859-1>

  Right now, it scans for charset	

*/


MODULE_PRIVATE int PeekMetaCharsetTag(char *pSrc, int len)
{
	char *p;
	char charset[MAX_CSNAME+1];
	int  i, k;
	int  n;
	char ch;


	for (i = 0; i < len; i++, pSrc++)
	{
		if (*pSrc != '<')
			continue;

		pSrc ++ ;
		i ++ ;
		if ((i+13) > len)  /* at least need more than 13 bytes */
			break;
		switch (*pSrc)	{
		case 'm':
		case 'M':
			if (strncasecomp(pSrc, "meta", 4) == 0)
			{
				pSrc = pSrc + 4; 
				i += 4;

				n = len - i;  /* set n here to make sure it won't go 
			                     out of block boundary  */
				for (p = pSrc; n > 0; p ++, n--)
				{
					if (*p == '>' || *p == '<')
						break ;

					if (n < 9)
					{ 	
						n = 0;
						break;
					}

					if ((n > 9) && (*p == 'c' || *p == 'C') && strncasecomp(p, "charset", 7) == 0)
					{
						p += 7;
						n -= 7;
						while ((n > 0) && (*p == ' ' || *p == '\t')) /* skip spaces */
						{
							p ++ ;
							n --;
						}
						if (n <= 0 || *p != '=')
							break;
						p ++;
						n --;
						while ((n > 0) && (*p == ' ' || *p == '\t')) /* skip spaces */
						{
							n --;
							p ++ ;
						}
					
						if (n <= 0)
							break;
						/* now we go to find real charset name and convert it to csid */
						if (*p == '\"' || *p == '\'')
						{
							p ++ ;
							n -- ;
							for (k = 0; n > 0 && k < MAX_CSNAME && *p != '\'' && *p != '\"'; p++, k++, n--)
								charset[k] = *p ;
							charset[k] = '\0';
						}
						else
						{
							for (k = 0; n > 0 && k < MAX_CSNAME && *p != '\'' && *p != '\"' && *p != '>'; p++, k++, n--)
								charset[k] = *p ;
							charset[k] = '\0';
						}

						if (*charset && (n >= 0))	/* ftang : change from if (*charset && (n > 0)) */
						{
							int16 doc_csid = INTL_CharSetNameToID(charset);

							if (doc_csid == CS_UNKNOWN)
							{
								doc_csid = CS_DEFAULT;
							}
							return doc_csid;
						}

					}
				}
				if (n == 0)  /* no more data left  */
					return 0;
			}
			break;
		case 'b':
		case 'B':			/* quit if we see <BODY ...> tag */
			if (strncasecomp(pSrc, "body", 4) == 0)
				return 0;
			break;
		case 'p':
		case 'P':			/* quit if we see <P ...> tag */
			ch = *(pSrc + 1);
			if (ch == '>' || ch == ' ' || ch == '\t')
				return 0;
			break;
		}
	}

	return 0;
}


