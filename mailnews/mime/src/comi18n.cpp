/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsICharsetConverterManager.h"
#include "nsICharsetAlias.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "prtypes.h"
#include "prmem.h"
#include "prlog.h"
#include "plstr.h"
#include "nsCRT.h"
#include "comi18n.h"


#include "nsIServiceManager.h"
#include "nsIStringCharsetDetector.h"
#include "nsIPref.h"
#include "mimebuf.h"
#include "nsMsgI18N.h"
#include "nsMimeTypes.h"
#include "nsICharsetConverterManager2.h"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

// BEGIN EXTERNAL DEPENDANCY

extern "C"  char * MIME_StripContinuations(char *original);
static PRBool intl_is_legal_utf8(const char *input, unsigned len);

////////////////////////////////////////////////////////////////////////////////
//  Pasted from the old code (xp_wrap.c)
//  Removed multi byte support because we use UTF-8 internally and no overwrap with us-ascii.

#undef OUTPUT
#define OUTPUT(b) \
do \
{ \
	if (out - ret >= size) \
	{ \
		unsigned char *old; \
		size *= 2; \
		old = ret; \
		ret = (unsigned char *) PR_REALLOC(old, size); \
		if (!ret) \
		{ \
			PR_Free(old); \
			return NULL; \
		} \
		out = ret + (size / 2); \
	} \
	*out++ = b; \
} while (0)

#undef OUTPUTSTR
#define OUTPUTSTR(s) \
do \
{ \
	const char *p1 = s; \
    while(*p1) {OUTPUT(*p1); p1++;} \
} while (0)
 

#undef OUTPUT_MACHINE_NEW_LINE
#ifdef XP_PC
#define OUTPUT_MACHINE_NEW_LINE(c) \
do \
{ \
	OUTPUT(nsCRT::CR); \
	OUTPUT(nsCRT::LF); \
} while (0)
#else
#ifdef XP_MAC
#define OUTPUT_MACHINE_NEW_LINE(c) \
do \
{ \
	OUTPUT(nsCRT::CR); \
	if (c) OUTPUT(nsCRT::LF); \
} while (0)
#else
#define OUTPUT_MACHINE_NEW_LINE(c) \
do \
{ \
	if (c) OUTPUT(nsCRT::CR); \
	OUTPUT(nsCRT::LF); \
} while (0)
#endif
#endif


#undef OUTPUT_NEW_LINE
#define OUTPUT_NEW_LINE(c) \
do \
{ \
	OUTPUT_MACHINE_NEW_LINE(c); \
	beginningOfLine = in; \
	currentColumn = 0; \
	lastBreakablePos = NULL; \
} while (0)


#undef NEW_LINE
#define NEW_LINE(c) \
do \
{ \
	if ((*in == nsCRT::CR) && (*(in + 1) == nsCRT::LF)) \
	{ \
		in += 2; \
	} \
	else \
	{ \
		in++; \
	} \
	OUTPUT_NEW_LINE(c); \
} while (0)

static unsigned char *
xp_word_wrap(unsigned char *str, int maxColumn, int checkQuoting,
			       const char *pfix, int addCRLF)
{
	unsigned char	*beginningOfLine;
	int		byteWidth;
	int		columnWidth;
	int		currentColumn;
	unsigned char	*end;
	unsigned char	*in;
	unsigned char	*lastBreakablePos;
	unsigned char	*out;
	unsigned char	*p;
	unsigned char	*ret;
	int32		size;

	if (!str)
	{
		return NULL;
	}

	size = 1;
	ret = (unsigned char *) PR_MALLOC(size);
	if (!ret)
	{
		return NULL;
	}

	in = str;
	out = ret;

	beginningOfLine = str;
	currentColumn = 0;
	lastBreakablePos = NULL;

	while (*in)
	{
		if (checkQuoting && (in == beginningOfLine) && (*in == '>'))
		{
			while (*in && (*in != nsCRT::CR) && (*in != nsCRT::LF))
			{
				OUTPUT(*in++);
			}
			if (*in)
			{
				NEW_LINE(addCRLF);
				if (pfix) OUTPUTSTR(pfix);
			}
			else
			{
				/*
				 * to prevent writing out line again after
				 * the main while loop
				 */
				beginningOfLine = in;
			}
		}
		else
		{
			if ((*in == nsCRT::CR) || (*in == nsCRT::LF))
			{
				if (in != beginningOfLine)
				{
					p = beginningOfLine;
					while (p < in)
					{
						OUTPUT(*p++);
					}
				}
				NEW_LINE(addCRLF);
				if (pfix) OUTPUTSTR(pfix);
				continue;
			}
			byteWidth = nsCRT::strlen((const char *) in);
			columnWidth = nsCRT::strlen((const char *) in);
			if (currentColumn + columnWidth > (maxColumn +
				(((*in == ' ') || (*in == '\t')) ? 1 : 0)))
			{
				if (lastBreakablePos)
				{
					p = beginningOfLine;
					end = lastBreakablePos - 1;
					if ((*end != ' ') && (*end != '\t'))
					{
						end++;
					}
					while (p < end)
					{
						OUTPUT(*p++);
					}
					if (addCRLF && (*end == ' ' || *end == '\t'))
					{
						OUTPUT(*end);
					}
					in = lastBreakablePos;
					OUTPUT_NEW_LINE(addCRLF);
					if (pfix) OUTPUTSTR(pfix);
					continue;
				}
			}
			if ( ((in[0] == ' ') || (in[0] == '\t')) &&
					((in[1] != ' ') && (in[1] != '\t')) )
			{
				lastBreakablePos = in + byteWidth;
			}
			in += byteWidth;
			currentColumn += columnWidth;
		}
	}

	if (in != beginningOfLine)
	{
		p = beginningOfLine;
		while (*p)
		{
			OUTPUT(*p++);
		}
	}

	OUTPUT(0);

	return ret;
}

////////////////////////////////////////////////////////////////////////////////

// The default setting of 'B' or 'Q' is the same as 4.5.
//
const char kMsgHeaderBEncoding[] = "B";
const char kMsgHeaderQEncoding[] = "Q";
static const char * intl_message_header_encoding(const char* charset)
{
  if (nsMsgI18Nmultibyte_charset(charset))
    return kMsgHeaderBEncoding;
  else
    return kMsgHeaderQEncoding;
}

static PRBool stateful_encoding(const char* charset)
{
  return (nsCRT::strcasecmp(charset, "ISO-2022-JP") == 0);
}

// END EXTERNAL DEPENDANCY


#define IS_MAIL_SEPARATOR(p) ((*(p) == ',' || *(p) == ' ' || *(p) == '\"' || *(p) == ':' || \
  *(p) == '(' || *(p) == ')' || *(p) == '\\' || (unsigned char)*p < 0x20))


/*
        Prototype for Private Function
*/
static PRBool  intlmime_only_ascii_str(const char *s);
static char *   intlmime_encode_next8bitword(char *src);

/*      we should consider replace this base64 encoding
function with a better one */
static char *   intlmime_decode_q(const char *in, unsigned length);
static char *   intlmime_decode_b(const char *in, unsigned length);
static int      intlmime_encode_base64 (const char *in, char *out);
static char *   intlmime_encode_base64_buf(char *subject, size_t size);
static char *   intlmime_encode_qp_buf(char *subject);

////////////////////////////////////////////////////////////////////////////////


static char basis_64[] =
   "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int intlmime_encode_base64 (const char *in, char *out)
{
  unsigned char c1, c2, c3;
  c1 = in[0];
  c2 = in[1];
  c3 = in[2];

  *out++ = basis_64[c1>>2];
  *out++ = basis_64[((c1 & 0x3)<< 4) | ((c2 & 0xF0) >> 4)];

  *out++ = basis_64[((c2 & 0xF) << 2) | ((c3 & 0xC0) >>6)];
  *out++ = basis_64[c3 & 0x3F];
  return 1;
}

static char *intlmime_encode_base64_buf(char *subject, size_t size)
{
  char *output = 0;
  char *pSrc, *pDest ;
  int i ;

  output = (char *)PR_Malloc(size * 4 / 3 + 4);
  if (output == NULL)
    return NULL;

  pSrc = subject;
  pDest = output ;
  for (i = size; i >= 3; i -= 3)
  {
    if (intlmime_encode_base64(pSrc, pDest) == 0) /* error */
    {
      pSrc += 3;
      pDest += 3;
    }
    else
    {
      pSrc += 3;
      pDest += 4;
    }
  }
  /* in case (i % 3 ) == 1 or 2 */
  if(i > 0)
  {
    char in[3];
    int j;
    in[0] = in[1] = in[2] ='\0';
    for(j=0;j<i;j++)
            in[j] = *pSrc++;
    intlmime_encode_base64(in, pDest);
    for(j=i+1;j<4;j++)
            pDest[j] = '=';
    pDest += 4;
  }
  *pDest = '\0';
  return output;
}

#define XX 127
/*
 * Table for decoding base64
 */
static char index_64[256] = {
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,62, XX,XX,XX,63,
    52,53,54,55, 56,57,58,59, 60,61,XX,XX, XX,XX,XX,XX,
    XX, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
    15,16,17,18, 19,20,21,22, 23,24,25,XX, XX,XX,XX,XX,
    XX,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
    41,42,43,44, 45,46,47,48, 49,50,51,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
};
#define CHAR64(c)  (index_64[(unsigned char)(c)])

static char *intlmime_decode_b (const char *in, unsigned length)
{
  char *out, *dest = 0;
  int c1, c2, c3, c4;

  out = dest = (char *)PR_Malloc(length+1);
  if (dest == NULL)
    return NULL;

  while (length > 0) {
    while (length > 0 && CHAR64(*in) == XX) {
      if (*in == '=') goto badsyntax;
      in++;
      length--;
    }
    if (length == 0) break;
    c1 = *in++;
    length--;
    
    while (length > 0 && CHAR64(*in) == XX) {
      if (*in == '=') goto badsyntax;
      in++;
      length--;
    }
    if (length == 0) goto badsyntax;
    c2 = *in++;
    length--;
    
    while (length > 0 && *in != '=' && CHAR64(*in) == XX) {
      in++;
      length--;
    }
    if (length == 0) goto badsyntax;
    c3 = *in++;
    length--;
    
    while (length > 0 && *in != '=' && CHAR64(*in) == XX) {
      in++;
      length--;
    }
    if (length == 0) goto badsyntax;
    c4 = *in++;
    length--;

    c1 = CHAR64(c1);
    c2 = CHAR64(c2);
	*out++ = ((c1<<2) | ((c2&0x30)>>4));
    if (c3 == '=') {
      if (c4 != '=') goto badsyntax;
      break; /* End of data */
    }
    c3 = CHAR64(c3);
    *out++ = (((c2&0x0F) << 4) | ((c3&0x3C) >> 2));
    if (c4 == '=') {
      break; /* End of data */
    }
    c4 = CHAR64(c4);
    *out++ = (((c3&0x03) << 6) | c4);
  }
  *out++ = '\0';
  return dest;

 badsyntax:
  PR_Free(dest);
  return NULL;
}

static char *intlmime_encode_qp_buf(char *subject)
{
  char *output = 0;
  unsigned char *p, *pDest ;
  int i, n, len ;

  if (subject == NULL || *subject == '\0')
    return NULL;
  len = nsCRT::strlen(subject);
  output = (char *) PR_Malloc(len * 3 + 1);
  if (output == NULL)
    return NULL;

  p = (unsigned char*)subject;
  pDest = (unsigned char*)output ;

  for (i = 0; i < len; i++)
  {
    /* XP_IS_ALPHA(*p) || XP_IS_DIGIT(*p)) */
    if ((*p < 0x80) &&
        (((*p >= 'a') && (*p <= 'z')) ||
         ((*p >= 'A') && (*p <= 'Z')) ||
         ((*p >= '0') && (*p <= '9')))
       )
      *pDest = *p;
    else
    {
      *pDest++ = '=';
      n = (*p & 0xF0) >> 4; /* high byte */
      if (n < 10)
        *pDest = '0' + n;
      else
        *pDest = 'A' + n - 10;
      pDest ++ ;

      n = *p & 0x0F;                  /* low byte */
      if (n < 10)
        *pDest = '0' + n;
      else
        *pDest = 'A' + n - 10;
    }

    p ++;
    pDest ++;
  }

  *pDest = '\0';
  return output;
}

static char *intlmime_encode_next8bitword(char *src)
{
  char *p;
  PRBool non_ascii = PR_FALSE;
  if (src == NULL)
    return NULL;
  p = src;
  while (*p == ' ')
    p ++ ;
  while ( *p )
  {
    if ((unsigned char) *p > 0x7F)
      non_ascii = PR_TRUE;
    if ( IS_MAIL_SEPARATOR(p) )
    {
      break;
    }
    p++; // the string is UTF-8 thus no conflict with scanning chars (which is all us-ascii).
  }

  if (non_ascii)
    return p;
  else
    return NULL;
}

/*      some utility function used by this file */
static PRBool intlmime_only_ascii_str(const char *s)
{
  for(; *s; s++)
    if(*s & 0x80)
      return PR_FALSE;
  return PR_TRUE;
}

static unsigned char * utf8_nextchar(unsigned char *str)
{
  if (*str < 128) {
    return (str+1);
  }
  int len = nsCRT::strlen((char *) str);
  // RFC 2279 defines more than 3 bytes sequences (0xF0, 0xF8, 0xFC),
  // but I think we won't encounter those cases as long as we're supporting UCS-2 and no surrogate.
  if ((len >= 3) && (*str >= 0xE0)) {
    return (str+3);
  }
  else if ((len >= 2) && (*str >= 0xC0)) {
    return (str+2);
  }

  return (str+1); // error, return +1 to avoid infinite loop
}

/*
lock then length of input buffer, so the return value is less than
iThreshold bytes
*/
static int ResetLen(int iThreshold, const char* buffer)
{
  unsigned char *begin, *end, *tmp;

  tmp = begin = end = (unsigned char *) buffer;
  PR_ASSERT( iThreshold > 1 );
  PR_ASSERT( buffer != NULL );
  while( (int) ( end - begin ) <= iThreshold ){
    tmp = end;
    if (!(*end))
      break;
    end = utf8_nextchar(end);
  }

  PR_ASSERT( tmp > begin );
  return tmp - begin;
}


static 
char * utf8_mime_encode_mail_address(char *charset, const char *src, int maxLineLen)
{
  char *begin, *end;
  char *retbuf = nsnull, *srcbuf = nsnull;
  char sep = '\0';
  char *sep_p = nsnull;
  PRInt32  retbufsize;
  PRInt32 line_len = 0;
  PRInt32 srclen;
  PRInt32 default_iThreshold;
  PRInt32 iThreshold;         /* how many bytes we can convert from the src */
  PRInt32 iEffectLen;         /* the maximum length we can convert from the src */
  PRInt32 mime2CharsLen;      /* length of charset name plus =? ?B? ?= chars  */
  PRBool bChop = PR_FALSE;
  PRBool bBase64Encode = PR_FALSE;

  if (!src)
    return nsnull;

  /* make a copy, so don't need touch original buffer */
  srcbuf = nsCRT::strdup(src);
  if (!srcbuf)
    return nsnull;
  begin = srcbuf;

  mime2CharsLen = strlen(charset) + 7;

  default_iThreshold = iEffectLen = ( maxLineLen - mime2CharsLen ) * 3 / 4;
  iThreshold = default_iThreshold;

  bBase64Encode = !nsCRT::strcasecmp(intl_message_header_encoding((const char *) charset), kMsgHeaderBEncoding);

  /* allocate enough buffer for conversion, this way it can avoid
     do another memory allocation which is expensive
   */
  retbufsize = nsCRT::strlen(srcbuf) * 3 + kMAX_CSNAME + 8;
  retbuf =  (char *) PR_Malloc(retbufsize);
  if (!retbuf) {  /* Give up if not enough memory */
    PR_Free(srcbuf);
    return nsnull;
  }

  *retbuf = '\0';

  // loop for separating encoded words by the separators
  // the input string is UTF-8 at this point
  srclen = strlen(srcbuf);

convert_and_encode:

  while (begin < (srcbuf + srclen))
  { /* get block of data between commas (and other separators) */
    char *p, *q;
    char *buf1, *buf2;
    PRInt32 len, newsize, convlen, retbuflen;
    PRBool non_ascii;

    retbuflen = strlen(retbuf);
    end = nsnull;

    /* scan for separator, conversion (and encode) happens on 8bit word between separators
       we need to exclude RFC822 special characters from encoded word 
       regardless of the encoding method ('B' or 'Q').
     */
    if (IS_MAIL_SEPARATOR(begin))
    {   /*  skip white spaces and separator */
        q = begin;
        while ( IS_MAIL_SEPARATOR(q) )
          q ++ ;
        sep = *(q - 1);
        sep_p = (q - 1);
        *(q - 1) = '\0';
        end = q - 1;
    }
    else
    {
      sep = '\0';
      /* scan for next separator */
      non_ascii = PR_FALSE;
      for (q = begin; *q;)
      {
        if ((unsigned char) *q > 0x7F)
          non_ascii = PR_TRUE;
        if ( IS_MAIL_SEPARATOR(q) )
        {
          if ((*q == ' ') && (non_ascii == PR_TRUE))
          {
            while ((p = intlmime_encode_next8bitword(q)) != NULL)
            {
              q = p;
              if (*p != ' ')
                 break;
            }
          }
          sep = *q;
          sep_p = q;
          *q = '\0';
          end = q;
          break;
        }
        q++;  // the string is UTF-8 thus no conflict with scanning chars (which is all us-ascii).
      }
    }

    // convert UTF-8 to mail charset
    /* get the to_be_converted_buffer's len */
//    len = nsCRT::strlen(begin);
    len = q - begin;

    if ( !intlmime_only_ascii_str(begin) )
    {
      // now the input is UTF-8, a character may be more than 2 bytes len
      // so we may over estimate (i.e. threshold may be smaller) but wrapping early is no problem, I think.

      /*
        the 30 length is calculated as follows (I think)
        total:             30 = 7 + 11 + 8 + 4
        --------------------------------------
        Mime Part II tags: 7  = "=?...?B?...?="
        Charset name:      11 = "iso-2022-jp"
        JIS excape seq.    8  = "<ESC>$B" + "<ESC>(B" * 4/3
        space for one char 4  = 2 * 4/3 rounded up to nearest 4
        Brian Stell 10/97
       */
      if ( ( maxLineLen - line_len < 30 ) || bChop ) {
        /* chop first, then continue */
        buf1 = retbuf + retbuflen;
        *buf1++ = nsCRT::CR;   *buf1++ = nsCRT::LF; *buf1++ = '\t';
        line_len = 0;
        retbuflen += 3;
        *buf1 = '\0';
        bChop = PR_FALSE;
        iThreshold = default_iThreshold;
      }

      // loop for line wrapping: estimate converted/encoded length 
      // and apply conversion (UTF-8 to mail charset)

      /* iEffectLen - the max byte-string length of JIS ( converted form S-JIS )
         name - such as "iso-2022-jp", the encoding name, MUST be shorter than 23 bytes
         mime2CharsLen - is the "=?:?:?=" 
       */
      iEffectLen = ( maxLineLen - line_len - mime2CharsLen ) * 3 / 4;
      while ( PR_TRUE ) {
        PRInt32 iBufLen;    /* converted buffer's length, not BASE64 */
        if ( len > iThreshold )
        {
          len = ResetLen(iThreshold, begin);
          if (sep_p) {
              *sep_p = sep; /*restore the original character */
              sep = '\0';   /*to be processed with the rest  */
              sep_p = nsnull; /*of this chunk in next iteration*/
          }
        }

        if ( iThreshold <= 1 )
        {
          /* Certain trashed mailboxes were causing an
          ** infinite loop at this point, so we need a way of
          ** getting out of trouble.
          **
          ** BEFORE:    iThreshold was becoming 1, then 0, and
          **            we were looping indefinitely.
          ** AFTER:     Now, first there will be
          **            an assert in the previous call to ResetLen,
          **            then we'll do that again on the repeat pass,
          **            then we'll exit more or less gracefully.
          **    - bug #83204, an oldie but goodie.
          **    - jrm 98/03/25
          */
          return nsnull;
        }
        // UTF-8 to mail charset conversion (or iso-8859-1 in case of us-ascii).
        nsresult rv = nsMsgI18NSaveAsCharset(TEXT_PLAIN, 
                                             !nsCRT::strcasecmp(charset, "us-ascii") ? "ISO-8859-1" : charset, 
                                             NS_ConvertUTF8toUCS2(begin, len).get(), &buf1);
        if (NS_FAILED(rv) || !buf1) {
          PR_FREEIF(srcbuf);
          PR_FREEIF(retbuf);
          return nsnull; //error
        }
        iBufLen = strlen(buf1);
//        buf1 = (char *) cvtfunc(obj, (unsigned char *)begin, len);
//        iBufLen = nsCRT::strlen( buf1 );
        if (iBufLen <= 0) {
          if (!end) {
            PR_FREEIF(srcbuf);
            PR_FREEIF(retbuf);
            return nsnull; //error
          }
          begin = end + 1;
          goto convert_and_encode;  // nothing was converted, skip this part
        }

        /* recal iThreshold each time based on last experience */
        iThreshold = len * iEffectLen / iBufLen;
        if ( iBufLen > iEffectLen ){
          /* the converted buffer is too large, we have to
                  1. free the buffer;
                  2. redo again based on the new iThreshold
          */
          bChop = PR_TRUE;           /* append CRLFTAB */
          if (buf1 && (buf1 != begin)){
            PR_Free(buf1);
            buf1 = nsnull;
          }
        } else {
          end = begin + len - 1;
          break;
        }
      }
      if (bChop && sep_p) {
        *sep_p = sep;   /* we are length limited so we do not need this */
        sep = '\0';     /* artifical terminator. So, restore the original character */
        sep_p = nsnull;
      }

      if (!buf1)
      {
        PR_Free(srcbuf);
        PR_Free(retbuf);
        return nsnull;
      }

      // converted to mail charset, now apply MIME encode
      if (bBase64Encode)
      {
        /* converts to Base64 Encoding */
        buf2 = (char *)intlmime_encode_base64_buf(buf1, nsCRT::strlen(buf1));
      }
      else
      {
        /* Converts to Quote Printable Encoding */
        buf2 = (char *)intlmime_encode_qp_buf(buf1);
      }


      if (buf1 && (buf1 != begin))
        PR_Free(buf1);

      if (!buf2) /* QUIT if memory allocation failed */
      {
        PR_Free(srcbuf);
        PR_Free(retbuf);
        return nsnull;
      }

      /* realloc memory for retbuff if necessary,
         mime2CharsLen: =?<charset>?B?..?=, 3: CR LF TAB */
      convlen = strlen(buf2) + mime2CharsLen;
      newsize = convlen + retbuflen + 3 + 2;  /* 2:SEP '\0', 3:CRLFTAB */

      if (newsize > retbufsize)
      {
        char *tempbuf;
        tempbuf = (char *) PR_Realloc(retbuf, newsize);
        if (!tempbuf)  /* QUIT, if not enough memory left */
        {
          PR_Free(buf2);
          PR_Free(srcbuf);
          PR_Free(retbuf);
          return nsnull;
        }
        retbuf = tempbuf;
        retbufsize = newsize;
      }
      /* buf1 points to end of current retbuf */
      buf1 = retbuf + retbuflen;

      if ((line_len > 10) &&
          ((line_len + convlen) > maxLineLen))
      {
        *buf1++ = nsCRT::CR;
        *buf1++ = nsCRT::LF;
        *buf1++ = '\t';
        line_len = 0;
        iThreshold = default_iThreshold;
      }
      *buf1 = '\0';

      /* Add encoding tag for base64 and QP */
      PL_strcat(buf1, "=?");
      PL_strcat(buf1, charset );
      if (bBase64Encode)
        PL_strcat(buf1, "?B?");
      else
        PL_strcat(buf1, "?Q?");
      PL_strcat(buf1, buf2);
      PL_strcat(buf1, "?=");

      line_len += convlen + 1;  /* 1: SEP */

      PR_Free(buf2);  /* free base64 buffer */
    }
    else  /* if no 8bit data in the block */  // so no conversion/encode needed
    {
      newsize = retbuflen + len + 2 + 3; /* 2: ',''\0', 3: CRLFTAB */
      if (newsize > retbufsize)
      {
        char *tempbuf;
        tempbuf = (char *) PR_Realloc(retbuf, newsize);
        if (!tempbuf)
        {
          PR_Free(srcbuf);
          PR_Free(retbuf);
          return nsnull;
        }
        retbuf = tempbuf;
        retbufsize = newsize;
      }
      buf1 = retbuf + retbuflen;

      if ((line_len > 10) &&
          ((line_len + len) > maxLineLen))
      {
        *buf1++ = nsCRT::CR;
        *buf1++ = nsCRT::LF;
        *buf1++ = '\t';
        line_len = 0;
        iThreshold = default_iThreshold;
      }
      /* copy buffer from begin to buf1 stripping CRLFTAB */
      for (p = begin; *p; p++)
      {
        if (*p == nsCRT::CR || *p == nsCRT::LF || *p == TAB)
          len --;
        else
          *buf1++ = *p;
      }
      *buf1 = '\0';
      line_len += len + 1;  /* 1: SEP */
    }

    buf1 = buf1 + nsCRT::strlen(buf1);
    if (sep == nsCRT::CR || sep == nsCRT::LF || sep == TAB) /* strip CR,LF,TAB */
      *buf1 = '\0';
    else
    {
      *buf1 = sep;
      *(buf1+1) = '\0';
    }

    if (end == NULL)
      break;
    begin = end + 1;
    if ('\0' == *begin)
      begin++;
  }

  if (srcbuf)
    PR_Free(srcbuf);

  return retbuf;
}

/*
  Latin1, latin2:
    Source --> Quote Printable --> Encoding Info
  Japanese:
    EUC,JIS,SJIS --> JIS --> Base64 --> Encoding Info
  Others:
    Use MIME flag:  0:  8bit on
                    1:  mime_use_quoted_printable_p
  return: NULL  if no conversion occured

*/

// input UTF-8, return NULL in case of error.
static
char *utf8_EncodeMimePartIIStr(const char *subject, char *charset, int maxLineLen)
{
  char *buf = NULL;

  if (subject == NULL)
    return NULL;

  /* check to see if subject are all ascii or not */
  if((*subject == '\0') ||
     (!intl_is_legal_utf8(subject, nsCRT::strlen(subject))) ||
    (!stateful_encoding((const char *) charset) && intlmime_only_ascii_str(subject)))
    return (char *) xp_word_wrap((unsigned char *) subject, maxLineLen, 0, " ", 1);

  /* If we are sending JIS then check the pref setting and
   * decide if we should convert hankaku (1byte) to zenkaku (2byte) kana.
   */
  if (!nsCRT::strcasecmp(charset, "ISO-2022-JP")) {
    ;
  }

  /* MIME Part2 encode */
  buf = utf8_mime_encode_mail_address(charset, subject, maxLineLen);

  return buf;
}

/*
 * Table for decoding hexadecimal in quoted-printable
 */
static char index_hex[256] = {
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
     0, 1, 2, 3,  4, 5, 6, 7,  8, 9,XX,XX, XX,XX,XX,XX,
    XX,10,11,12, 13,14,15,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,10,11,12, 13,14,15,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
};
#define HEXCHAR(c)  (index_hex[(unsigned char)(c)])

static char *intlmime_decode_q(const char *in, unsigned length)
{
  char *out, *dest = 0;

  out = dest = (char *)PR_Malloc(length+1);
  if (dest == NULL)
    return NULL;
  memset(out, 0, length+1);
  while (length > 0) {
    switch (*in) {
    case '=':
      if (length < 3 || HEXCHAR(in[1]) == XX ||
          HEXCHAR(in[2]) == XX) goto badsyntax;
      *out++ = (HEXCHAR(in[1]) << 4) + HEXCHAR(in[2]);
      in += 3;
      length -= 3;
      break;

    case '_':
      *out++ = ' ';
      in++;
      length--;
      continue;

    default:
      if (*in & 0x80) goto badsyntax;
      *out++ = *in++;
      length--;
    }
  }
  *out++ = '\0';
  return dest;

 badsyntax:
  PR_Free(dest);
  return NULL;
}

static PRBool intl_is_legal_utf8(const char *input, unsigned len)
{
  int c;

  while (len) {
    c = (unsigned char)*input++;
    len--;
    if (c == 0x1B) return PR_FALSE;
    if ((c & 0x80) == 0) continue;
    if ((c & 0xE0) == 0xC0) {
      if (len < 1 || (*input & 0xC0) != 0x80 ||
        ((c & 0x1F)<<6) + (*input & 0x3f) < 0x80) {
        return PR_FALSE;
      }
      input++;
      len--;
    } else if ((c & 0xF0) == 0xE0) {
      if (len < 2 || (input[0] & 0xC0) != 0x80 ||
        (input[1] & 0xC0) != 0x80) {
        return PR_FALSE;
      }
      input += 2;
      len -= 2;
    } else if ((c & 0xF8) == 0xF0) {
      if (len < 3 || (input[0] & 0xC0) != 0x80 ||
        (input[1] & 0xC0) != 0x80 || (input[2] & 0xC0) != 0x80) {
        return PR_FALSE;
      }
      input += 2;
      len -= 2;
    } else {
      return PR_FALSE;
    }
  }
  return PR_TRUE;
}

static void intl_copy_uncoded_header(char **output, const char *input,
  unsigned len, const char *default_charset)
{
  int c;
  char *dest = *output;

  if (!default_charset) {
    memcpy(dest, input, len);
    *output = dest + len;
    return;
  }

  // Copy as long as it's US-ASCII.  An ESC may indicate ISO 2022
  while (len && (c = (unsigned char)*input++) != 0x1B && !(c & 0x80)) {
    *dest++ = c;
    len--;
  }
  if (!len) {
    *output = dest;
    return;
  }
  input--;

  // If not legal UTF-8, treat as default charset
  nsAutoString tempUnicodeString;
  if (!intl_is_legal_utf8(input, len) &&
      NS_SUCCEEDED(ConvertToUnicode(default_charset, nsCAutoString(input, len).get(), tempUnicodeString))) {
    NS_ConvertUCS2toUTF8 utf8_text(tempUnicodeString);
    PRInt32 output_len = utf8_text.Length();
    memcpy(dest, utf8_text.get(), output_len);
    *output = dest + output_len;
  } else {
    memcpy(dest, input, len);
    *output = dest + len;
  }
}

static const char especials[] = "()<>@,;:\\\"/[]?.=";

static
char *intl_decode_mime_part2_str(const char *header,
  const char *default_charset, PRBool override_charset)
{
  char *output_p = NULL;
  char *retbuff = NULL;
  const char *p, *q, *r;
  char *decoded_text;
  const char *begin; /* tracking pointer for where we are in the input buffer */
  int last_saw_encoded_word = 0;
  const char *charset_start, *charset_end;
  char charset[80];
  nsAutoString tempUnicodeString;

  // initialize charset name to an empty string
  charset[0] = '\0';

  /* Assume no more than 3X expansion due to UTF-8 conversion */
  retbuff = (char *)PR_Malloc(3*nsCRT::strlen(header)+1);

  if (retbuff == NULL)
    return NULL;

  output_p = retbuff;
  begin = header;

  while ((p = PL_strstr(begin, "=?")) != 0) {
    if (last_saw_encoded_word) {
      /* See if it's all whitespace. */
      for (q = begin; q < p; q++) {
        if (!PL_strchr(" \t\r\n", *q)) break;
      }
    }

    if (!last_saw_encoded_word || q < p) {
      /* copy the part before the encoded-word */
      intl_copy_uncoded_header(&output_p, begin, p - begin, default_charset);
      begin = p;
    }

    p += 2;

    /* Get charset info */
    charset_start = p;
    charset_end = 0;
    for (q = p; *q != '?'; q++) {
      if (*q <= ' ' || PL_strchr(especials, *q)) {
        goto badsyntax;
      }

      /* RFC 2231 section 5 */
      if (!charset_end && *q == '*') {
        charset_end = q; 
      }
    }
    if (!charset_end) {
      charset_end = q;
    }

    /* Check for too-long charset name */
    if ((unsigned)(charset_end - charset_start) >= sizeof(charset)) goto badsyntax;
    
    memcpy(charset, charset_start, charset_end - charset_start);
    charset[charset_end - charset_start] = 0;

    q++;
    if (*q != 'Q' && *q != 'q' && *q != 'B' && *q != 'b')
      goto badsyntax;

    if (q[1] != '?')
      goto badsyntax;

    r = q;
    for (r = q + 2; *r != '?'; r++) {
      if (*r < ' ') goto badsyntax;
    }
    if (r[1] != '=')
	    goto badsyntax;
    else if (r == q + 2) {
	    // it's empty, skip
	    begin = r + 2;
	    last_saw_encoded_word = 1;
	    continue;
    }


    if(*q == 'Q' || *q == 'q')
      decoded_text = intlmime_decode_q(q+2, r - (q+2));
    else
      decoded_text = intlmime_decode_b(q+2, r - (q+2));

    if (decoded_text == NULL)
      goto badsyntax;

    // Override charset if requested.  Never override labeled UTF-8.
    if (override_charset && 0 != nsCRT::strcasecmp(charset, "UTF-8")) {
      PL_strcpy(charset, default_charset);
    }

    if (NS_SUCCEEDED(ConvertToUnicode(charset, decoded_text, tempUnicodeString))) {
      NS_ConvertUCS2toUTF8 utf8_text(tempUnicodeString.get());
      PRInt32 utf8_len = utf8_text.Length();

      memcpy(output_p, utf8_text.get(), utf8_len);
      output_p += utf8_len;
    } else {
      PL_strcpy(output_p, "\347\277\275"); /* UTF-8 encoding of U+FFFD */
      output_p += 3;
    }

    PR_Free(decoded_text);

    begin = r + 2;
    last_saw_encoded_word = 1;
    continue;

  badsyntax:
    /* copy the part before the encoded-word */
    PL_strncpy(output_p, begin, p - begin);
    output_p += p - begin;
    begin = p;
    last_saw_encoded_word = 0;
  }

  /* put the tail back  */
  intl_copy_uncoded_header(&output_p, begin, nsCRT::strlen(begin), default_charset);
  *output_p = '\0';

  return retbuff;
}


////////////////////////////////////////////////////////////////////////////////
// BEGIN PUBLIC INTERFACE
extern "C" {
#define PUBLIC


extern "C" char *MIME_DecodeMimeHeader(const char *header, 
                                       const char *default_charset,
                                       PRBool override_charset,
                                       PRBool eatContinuations)
{
  char *result = nsnull;

  if (header == 0)
    return nsnull;

  // If no MIME encoded then do nothing otherwise decode the input.
  if (PL_strstr(header, "=?") ||
      (default_charset && !intl_is_legal_utf8(header, nsCRT::strlen(header)))) {
	  result = intl_decode_mime_part2_str(header, default_charset, override_charset);
  } else if (eatContinuations && 
             (PL_strchr(header, '\n') || PL_strchr(header, '\r'))) {
    result = nsCRT::strdup(header);
  } else {
    eatContinuations = PR_FALSE;
  }
  if (eatContinuations)
    result = MIME_StripContinuations(result);

  return result;
}  

char *MIME_EncodeMimePartIIStr(const char* header, const char* mailCharset, const PRInt32 encodedWordSize)
{
  return utf8_EncodeMimePartIIStr((char *) header, (char *) mailCharset, encodedWordSize);
}

// UTF-8 utility functions.

char * NextChar_UTF8(char *str)
{
  return (char *) utf8_nextchar((unsigned char *) str);
}

//detect charset soly based on aBuf. return in aCharset
nsresult
MIME_detect_charset(const char *aBuf, PRInt32 aLength, const char** aCharset)
{
  nsresult res;
  char theBuffer[128];
  CBufDescriptor theBufDecriptor( theBuffer, PR_TRUE, sizeof(theBuffer), 0);
  nsCAutoString detector_contractid(theBufDecriptor);
  nsXPIDLString detector_name;
  nsCOMPtr<nsIStringCharsetDetector> detector;
  *aCharset = nsnull;

  detector_contractid.Assign(NS_STRCDETECTOR_CONTRACTID_BASE);

  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &res)); 
  if (NS_SUCCEEDED(res)) {
    if (NS_SUCCEEDED(prefs->GetLocalizedUnicharPref("intl.charset.detector", getter_Copies(detector_name)))) {
      detector_contractid.Append(NS_ConvertUCS2toUTF8(detector_name).get());
    }
  }

  if (detector_contractid.Length() > sizeof(NS_STRCDETECTOR_CONTRACTID_BASE)) {
    detector = do_CreateInstance(detector_contractid.get(), &res);
    if (NS_SUCCEEDED(res)) {
      nsDetectionConfident oConfident;
      res = detector->DoIt(aBuf, aLength, aCharset, oConfident);
      if (NS_SUCCEEDED(res) && (eBestAnswer == oConfident || eSureAnswer == oConfident)) {
        return NS_OK;
      }
    }
  }
  return res;
}

//Get unicode decoder(from inputcharset to unicode) for aInputCharset
nsresult 
MIME_get_unicode_decoder(const char* aInputCharset, nsIUnicodeDecoder **aDecoder)
{
  nsresult res;

  // get charset converters.
  nsCOMPtr<nsICharsetConverterManager2> ccm2 = 
           do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &res); 
  if (NS_SUCCEEDED(res)) {
    nsCOMPtr <nsIAtom> charsetAtom;
    if (!*aInputCharset || !nsCRT::strcasecmp("us-ascii", aInputCharset))
      res = ccm2->GetCharsetAtom(NS_LITERAL_STRING("ISO-8859-1").get(), getter_AddRefs(charsetAtom));
    else
      res = ccm2->GetCharsetAtom(NS_ConvertASCIItoUCS2(aInputCharset).get(), getter_AddRefs(charsetAtom));
    // create a decoder (conv to unicode), ok if failed if we do auto detection
    if (NS_SUCCEEDED(res))
      res = ccm2->GetUnicodeDecoder(charsetAtom, aDecoder);
  }
   
  return res;
}

//Get unicode encoder(from unicode to inputcharset) for aOutputCharset
nsresult 
MIME_get_unicode_encoder(const char* aOutputCharset, nsIUnicodeEncoder **aEncoder)
{
  nsresult res;

  // get charset converters.
  nsCOMPtr<nsICharsetConverterManager2> ccm2 = 
           do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &res); 
  if (NS_SUCCEEDED(res)) {
    nsCOMPtr <nsIAtom> charsetAtom;
    if (*aOutputCharset) {
      res = ccm2->GetCharsetAtom(NS_ConvertASCIItoUCS2(aOutputCharset).get(), getter_AddRefs(charsetAtom));

      // create a encoder (conv from unicode)
      if (NS_SUCCEEDED(res))
        res = ccm2->GetUnicodeEncoder(charsetAtom, aEncoder);
    }
  }
   
  return res;
}

} /* end of extern "C" */
// END PUBLIC INTERFACE

