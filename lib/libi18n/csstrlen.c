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
/*	csstrlen.c	*/
/*
	Routines that tell you information about one csid
*/
#include "intlpriv.h"

/*	csinfoindex and csinfo_tbl work together for performance inprovement. 
	Whenever you add an entry inside csinfo_tbl, you also need to change
	csinfoindex
*/
#define MAX_FIRSTBYTE_RANGE  3
typedef struct {
    struct {
        unsigned char bytes;       /* number of bytes for range */
        unsigned char columns;     /* number of columns for range */
        unsigned char range[2];    /* Multibyte first byte range */
    } enc[MAX_FIRSTBYTE_RANGE];
} csinfo_t;

PRIVATE csinfo_t csinfo_tbl[] =
{
/* b = bytes; c = columns                                               */
/*				b c  range 1       b c  range 2       b c  range 3     */
	/*  0 */ {{{2,2,{0x81,0x9f}}, {2,2,{0xe0,0xfc}}, {0,0,{0x00,0x00}}}},	/* For SJIS */
	/*  1 */ {{{2,2,{0xa1,0xfe}}, {2,1,{0x8e,0x8e}}, {3,2,{0x8f,0x8f}}}},	/* For EUC_JP */
	/*  2 */ {{{2,2,{0xa1,0xfe}}, {0,0,{0x00,0x00}}, {0,0,{0x00,0x00}}}},	/* For BIG5 GB KSC */
	/*  3 */ {{{2,2,{0xa1,0xfe}}, {4,2,{0x8e,0x8e}}, {0,0,{0x00,0x00}}}},	/* For CNS_8BIT */
	/*  4 */ {{{2,2,{0x21,0x7e}}, {0,0,{0x00,0x00}}, {0,0,{0x00,0x00}}}},	/* For 2 Byte GL */
	/*  5 */ {{{2,2,{0xC0,0xDF}}, {3,2,{0xE0,0xEF}}, {0,0,{0x00,0x00}}}},	/* For UTF8 */
	/*  0 */ {{{0,0,{0x00,0x00}}, {0,0,{0x00,0x00}}, {0,0,{0x00,0x00}}}}
};
/*	Array to index from the lower 8 bits of csid into the index of csinfo_tbl */
PRIVATE int csinfoindex[256] =
{/*	0	1	2	3	4	5	6	7	8	9	a	b	c	d	e	f	*/	
	-1,	-1,	-1,	-1,	0,	1,	-1,	2,	2,	3,	-1,	-1,	2,	-1,	-1,	-1,	/*	0x00 */
	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	4,	4,	4,	4,	-1,	4,	4,	-1,	/*	0x10 */
	-1,	-1,	5,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	/*	0x20 */
	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	/*	0x30 */
	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	/*	0x40 */
	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	/*	0x50 */
	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	/*	0x60 */
	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	/*	0x70 */
	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	/*	0x80 */
	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	/*	0x90 */
	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	/*	0xa0 */
	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	/*	0xb0 */
	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	/*	0xc0 */
	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	/*	0xd0 */
	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	/*	0xe0 */
	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	-1,	/*	0xf0 */
};
#define INTL_GETTBLINDEX(csid)	(csinfoindex[ (csid) & 0x00FF ])

PRIVATE csinfo_t* intl_GetInfoTbl(int16 csid)
{
	int idx = INTL_GETTBLINDEX(csid);
	if(idx < 0)	
		return NULL;
	else
		return &csinfo_tbl[idx];
}

/***********************************************************
 INTL_MidTruncateString  truncate a string removing the
						 middle
 Input:  int16 csid  Char Set ID
         char *input un-truncated string

 Output: char *output pointer to truncated string buffer

***********************************************************/

PUBLIC void
INTL_MidTruncateString (int16 csid, const char *input, char *output, int max_length)
{
  char *begin_part, *p;
  int L = strlen (input);
  char *tmp = 0;
  int begin_len, mid, rem;

  /*
   * If it fits then no need to truncate
   */
  if (L <= max_length)
    {
      strcpy (output, input);
      return;
    }

  if (input == output) /* if copying in place use tmp buf */
    {
      tmp = output;
      output = (char *) calloc (1, max_length + 1);
    }

  /* 
   * find the 1st half
   */
  mid = (max_length - 3) / 2; /* approx 1st half */
  /* find 1st half to whole char */
  for (begin_part=p=(char*)input; 
		*p && p<=((char*)input+mid); p=INTL_NextChar(csid, p))
    begin_part = p; /* remember last good point before mid */
  /* exact mid point */
  begin_len = begin_part - input;

  /*
   * Copy 1st half
   */
  strncpy (output, input, begin_len);
  strncpy (output + begin_len, "...", 3);

  /* 
   * find the remainder
   */
  rem = L - mid; /* approx remainder */
  /* find remainder to whole char */
  for (p=begin_part; *p && p<((char*)input+rem); p=INTL_NextChar(csid, p))
    continue;
  /* exact remainder */
  rem = p - input;
  strncpy (output + begin_len + 3, p, L - rem + 1);

  if (tmp)
    {
      strncpy (tmp, output, max_length + 1);
      free (output);
    }
}
/***********************************************************
 Input:  int (int16) charsetid		Char Set ID
         char *pstr				   Buffer which always point to Multibyte char first byte
                      			   or normal single byte char
 Output: return next char position
***********************************************************/
PUBLIC char * INTL_NextChar(int charsetid, char *pstr)
{
	csinfo_t	*pInfo ;
	unsigned char ch ;
	int i;

	if ((INTL_CharSetType(charsetid) == SINGLEBYTE) || (*pstr == 0)) /* If no csid, assume it's not multibyte */
		return pstr + 1;

	ch = *pstr ;
	if((pInfo = intl_GetInfoTbl((int16)charsetid)) != NULL)
	{
		for (i=0; i<MAX_FIRSTBYTE_RANGE && pInfo->enc[i].bytes > 0; i++)
		{
			if ((ch >= pInfo->enc[i].range[0]) && (ch <= pInfo->enc[i].range[1]))
			{	
				int j = 0;
				for (j=0; pstr[j] && j < pInfo->enc[i].bytes; j++)
					;
				if (j < pInfo->enc[i].bytes)
					return pstr+1;
				else
					return pstr+j;
			}
		}
		return pstr + 1;
	}
	return pstr + 1;
}

/********************************************************
 Input:  DocumentContext context Window Context
         unsigned char ch		 Buffer which always point to Multibyte char
								 first byte or normal single byte char
 Output: 1, if ch is under ShiftJIS type MultiByte first byte range
         2, if ch is under EUC type MultiByte first byte range
		 0, if it's not MultiByte firstbyte
*********************************************************/

PUBLIC
int PR_CALLBACK
INTL_IsLeadByte(int charsetid, unsigned char ch)
{
	csinfo_t	*pInfo ;
	int i;

	if ((INTL_CharSetType(charsetid) == SINGLEBYTE) || (ch == 0)) /* If no csid, assume it's not multibyte */
		return 0;

	if((pInfo = intl_GetInfoTbl((int16)charsetid)) != NULL)
	{
		for (i=0; i<MAX_FIRSTBYTE_RANGE && pInfo->enc[i].bytes > 0; i++)
			if ((ch >= pInfo->enc[i].range[0]) &&
				(ch <= pInfo->enc[i].range[1]))
				return pInfo->enc[i].bytes-1;
		return 0 ;
	}
	return 0;
}

PUBLIC int 
INTL_CharLen(int charsetid, unsigned char *pstr)
{
	int i,l;
	if ((!pstr) || (!*pstr)) return 0;
	l = 1 + INTL_IsLeadByte(charsetid, *pstr);
	for(i=1, pstr++ ; (i<l) && (*pstr); i++, pstr++)
		;
	return i;
}

PUBLIC int
INTL_ColumnWidth(int charsetid, unsigned char *str)
{
	unsigned char	b;
	csinfo_t	*pInfo;
	int		i;

	if ((!str) || (!*str))
		return 0;

	if (INTL_CharSetType(charsetid) == SINGLEBYTE)
		return 1;
	if((pInfo = intl_GetInfoTbl((int16)charsetid)) != NULL)
	{
		b = *str;
		for (i = 0; (i < MAX_FIRSTBYTE_RANGE) &&
			pInfo->enc[i].bytes; i++)
		{
			if ((b >= pInfo->enc[i].range[0]) &&
				(b <= pInfo->enc[i].range[1]))
			{
				return pInfo->enc[i].columns;
			}
		}
	}

	return 1;
}

/********************************************************
 Input:  int (int16) charsetid	Char Set ID
         char *pstr				Buffer which always point to Multibyte char
								first byte or normal single byte char
		 int  pos				byte position
 Output: 0,   if pos is not on kanji char
 		 1,   if pos is on kanji 1st byte
	     2,   if pos is on kanji 2nd byte
		 3,   if pos is on kanji 3rd byte
 Note:   Current this one only works for ShiftJis type multibyte not for JIS or EUC
*********************************************************/
PUBLIC int
INTL_NthByteOfChar(int charsetid, char *pstr, int pos)
{
	int	i;
	int	prev;

	pos--;

	if
	(
		(INTL_CharSetType(charsetid) == SINGLEBYTE) ||
		(!pstr)  ||
		(!*pstr) ||
		(pos < 0)
	)
	{
		return 0;
	}

	i = 0;
	prev = 0;
	while (pstr[i] && (i <= pos))
	{
		prev = i;
		i += INTL_CharLen(charsetid, (unsigned char *) &pstr[i]);
	}
	if (i <= pos)
	{
		return 0;
	}
	if (INTL_CharLen(charsetid, (unsigned char *) &pstr[prev]) < 2)
	{
		return 0;
	}

	return pos - prev + 1;
}

PUBLIC int
INTL_IsHalfWidth(uint16 win_csid, unsigned char *pstr)
{
	int	c;

	c = *pstr;

	switch (win_csid)
	{
	case CS_SJIS:
		if ((0xa1 <= c) && (c <= 0xdf))
		{
			return 1;
		}
		break;
	case CS_EUCJP:
		if (c == 0x8e)
		{
			return 1;
		}
		break;
	default:
		break;
	}

	return 0;
}


/*	
	INTL_NextCharIdxInText 
		Input: 	csid - window csid	
				text - point to a text buffer
				pos  - origional index position
		output: index of the position of next character
		Called by lo_next_character in layfind.c
*/
PUBLIC int INTL_NextCharIdxInText(int16 csid, unsigned char *text, int pos)
{
	return pos + INTL_CharLen(csid ,text+pos);
}
/*	
	INTL_PrevCharIdxInText 
		Input: 	csid - window csid	
				text - point to a text buffer
				pos  - origional index position
		output: index of the position of previous character
		Called by lo_next_character in layfind.c
*/
PUBLIC int INTL_PrevCharIdxInText(int16 csid, unsigned char *text, int pos)
{
	int rev, ff , thislen;
	if((INTL_CharSetType(csid) == SINGLEBYTE) ) {
		return pos - 1;
	}
	else 
	{
		/*	First, backward to character in ASCII range */
		for(rev=pos - 1; rev > 0 ; rev--)
		{
			if(((text[rev] & 0x80 ) == 0) &&
			   ((rev + INTL_CharLen(csid ,text+rev)) < pos))
				break;
		}
			
		/*	Then forward till we cross the position. */
		for(ff = rev ; ff < pos ; ff += thislen)
		{
			thislen = INTL_CharLen(csid ,text+ff);
			if((ff + thislen) >= pos)
				break;
		}
		return ff;
	}
}

/*	
	INTL_NextCharIdx
		Input: 	csid - window csid	
				text - point to a text buffer
				pos  - 0 based position
		output: 0 based next char position
		Note: this one works for any position no matter it's legal or not
*/

PUBLIC int INTL_NextCharIdx(int16 csid, unsigned char *str, int pos)
{
	int n;
	unsigned char *p;

	if((INTL_CharSetType(csid) == SINGLEBYTE) || (pos < 0))
		return pos + 1;

	n = INTL_NthByteOfChar(csid, (char *) str, pos+1);
	if (n == 0)
		return pos + 1;

	p = str + pos - n + 1;
	return pos + INTL_CharLen(csid, p) - n + 1;
}
/*	
	INTL_PrevCharIdx
		Input: 	csid - window csid	
				text - point to a text buffer
				pos  - 0 based position
		output: 0 based prev char position
		Note: this one works for any position no matter it's legal or not
*/
PUBLIC int INTL_PrevCharIdx(int16 csid, unsigned char *str, int pos)
{
	int n;
	if((INTL_CharSetType(csid) == SINGLEBYTE) || (pos <= 0))
		return pos - 1;
#ifdef DEBUG
	n = INTL_NthByteOfChar(csid, (char *) str, pos+1);
	if (n > 1)
	{
		XP_TRACE(("Wrong position passed to INTL_PrevCharIdx"));
		pos -= (n - 1); 
	}
#endif

	pos --;
	if ((n = INTL_NthByteOfChar(csid, (char *) str, pos+1)) > 1)
		return pos - n + 1;
	else
		return pos;
}



PUBLIC
int32  INTL_TextByteCountToCharLen(int16 csid, unsigned char* text, uint32 byteCount)
{
	/* quickly return if it is zero */
	if(byteCount == 0 )
		return 0;
	if(INTL_CharSetType(csid) == SINGLEBYTE)
	{
		/* for single byte csid, byteCount equal to charLen */
		return byteCount;
	}
	else
	{
		csinfo_t	*pInfo ;
		if((pInfo = intl_GetInfoTbl(csid)) != NULL)
		{
			uint32 curByte, curChar;
			int thislen;
			for(curByte=curChar=0; curByte < byteCount ;curChar++,curByte += thislen)
			{
				int i;
				unsigned char ch = text[curByte];
				/* preset thislen to 1 and looking for the entry for this char */
				for (i=0, thislen = 1; i<MAX_FIRSTBYTE_RANGE && pInfo->enc[i].bytes > 0; i++)
				{
					if ((ch >= pInfo->enc[i].range[0]) && (ch <= pInfo->enc[i].range[1]))
						thislen = pInfo->enc[i].bytes;
				}
			}
			return curChar;		
		}
	}
	/* it should not come to here */
	XP_ASSERT(byteCount);
	return byteCount;		
}



PUBLIC
int32  INTL_TextCharLenToByteCount(int16 csid, unsigned char* text, uint32 charLen)
{
	/* quickly return if it is zero */
	if(charLen == 0 )
		return 0;
	if(INTL_CharSetType(csid) == SINGLEBYTE)
	{
		/* for single byte csid, byteCount equal to charLen */
		return charLen;
	}
	else
	{
		csinfo_t	*pInfo ;
		if((pInfo = intl_GetInfoTbl(csid)) != NULL)
		{
			uint32 curByte, curChar;
			int thislen;
			for(curByte=curChar=0; curChar < charLen ;curChar++,curByte += thislen)
			{
				int i;
				unsigned char ch = text[curByte];
				/* preset thislen to 1 and looking for the entry for this char */
				for (i=0, thislen = 1; i<MAX_FIRSTBYTE_RANGE && pInfo->enc[i].bytes > 0; i++)
				{
					if ((ch >= pInfo->enc[i].range[0]) && (ch <= pInfo->enc[i].range[1]))
						thislen = pInfo->enc[i].bytes;
				}
			}
			return curByte;	
		}
	}
	/* it should not come to here */
	XP_ASSERT(charLen);
	return charLen;
}




