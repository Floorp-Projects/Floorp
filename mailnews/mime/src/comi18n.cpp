/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */
#define NS_IMPL_IDS
#include "nsICharsetConverterManager.h"
#include "nsICharsetAlias.h"
#undef NS_IMPL_IDS

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
#include "nsTextFormatter.h"
#include "nsMsgI18N.h"
#include "nsMimeTypes.h"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

// BEGIN EXTERNAL DEPENDANCY

extern "C"  char * MIME_StripContinuations(char *original);
/* These are needed by other libmime functions */
extern "C" PRInt16 INTL_DefaultWinCharSetID(char a) { return a; }
extern "C" char   *INTL_CsidToCharsetNamePt(PRInt16 id) { return "ISO-8859-1"; }
extern "C" PRInt16 INTL_CharSetNameToID(char *charset) { return 0; }


////////////////////////////////////////////////////////////////////////////////
//  Pasted from the old code (xp_wrap.c)
//  Removed multi byte support because we use utf-8 internally and no overwrap with us-ascii.

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
	OUTPUT(CR); \
	OUTPUT(LF); \
} while (0)
#else
#ifdef XP_MAC
#define OUTPUT_MACHINE_NEW_LINE(c) \
do \
{ \
	OUTPUT(CR); \
	if (c) OUTPUT(LF); \
} while (0)
#else
#define OUTPUT_MACHINE_NEW_LINE(c) \
do \
{ \
	if (c) OUTPUT(CR); \
	OUTPUT(LF); \
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
	if ((*in == CR) && (*(in + 1) == LF)) \
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
			while (*in && (*in != CR) && (*in != LF))
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
			if ((*in == CR) || (*in == LF))
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
  if (!nsCRT::strcasecmp(charset, "ISO-2022-JP"))
    return PR_TRUE;

  return PR_FALSE;
}

// END EXTERNAL DEPENDANCY


#define IS_MAIL_SEPARATOR(p) ((*(p) == ',' || *(p) == ' ' || *(p) == '\"' || *(p) == ':' || \
  *(p) == '(' || *(p) == ')' || *(p) == '\\' || (unsigned char)*p < 0x20))


/*
        Prototype for Private Function
*/
static PRBool  intlmime_only_ascii_str(const char *s);
static char *   intlmime_encode_next8bitword(char *src);

/*      we should consider replace this base64 decodeing and encoding
function with a better one */
static int      intlmime_decode_base64 (const char *in, char *out);
static char *   intlmime_decode_qp(char *in);
static int      intlmime_encode_base64 (const char *in, char *out);
static char *   intlmime_decode_base64_buf(char *subject);
static char *   intlmime_encode_base64_buf(char *subject, size_t size);
static char *   intlmime_encode_qp_buf(char *subject);


static PRBool intlmime_is_mime_part2_header(const char *header);
#if 0
static  char *  intl_decode_mime_part2_str(const char *, int , char* );
#endif

#if 0
/*      We probably should change these into private instead of PUBLIC */
static char *DecodeBase64Buffer(char *subject);
static char *EncodeBase64Buffer(char *subject, size_t size);
#endif

////////////////////////////////////////////////////////////////////////////////

#if 0
/* 4.0: Made Encode & Decode public for use by libpref; added size param.
 */
static char *EncodeBase64Buffer(char *subject, size_t size)
{
  /* This function should be obsolete */
  /* We should not make this public in libi18n */
  /* We should use the new Base64 Encoder wrote by jwz in libmime */
  return intlmime_encode_base64_buf(subject, size);
}

static char *DecodeBase64Buffer(char *subject)
{
  /* This function should be obsolete */
  /* We should not make this public in libi18n */
  /* We should use the new Base64 Decoder wrote by jwz in libmime */
  return intlmime_decode_base64_buf(subject);
}
#endif

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

static int intlmime_decode_base64 (const char *in, char *out)
{
  /* reads 4, writes 3. */
  int j;
  unsigned long num = 0;

  for (j = 0; j < 4; j++)
  {
    unsigned char c;
    if (in[j] >= 'A' && in[j] <= 'Z')              c = in[j] - 'A';
    else if (in[j] >= 'a' && in[j] <= 'z') c = in[j] - ('a' - 26);
    else if (in[j] >= '0' && in[j] <= '9') c = in[j] - ('0' - 52);
    else if (in[j] == '+')                                 c = 62;
    else if (in[j] == '/')                                 c = 63;
    else if (in[j] == '=')                                 c = 0;
    else
    {
      /*      abort ();       */
      PL_strcpy(out, in);        /* I hate abort */
      return 0;
    }
    num = (num << 6) | c;
  }

  *out++ = (unsigned char) (num >> 16);
  *out++ = (unsigned char) ((num >> 8) & 0xFF);
  *out++ = (unsigned char) (num & 0xFF);
  return 1;
}

static char *intlmime_decode_base64_buf(char *subject)
{
  char *output = 0;
  char *pSrc, *pDest ;
  int i ;

  mime_SACopy(&output, subject); /* Assume converted text are always less than source text */

  pSrc = subject;
  pDest = output ;
  for (i = nsCRT::strlen(subject); i > 3; i -= 4)
  {
    if (intlmime_decode_base64(pSrc, pDest) == 0)
    {
      pSrc += 4;
      pDest += 4;
    }
    else
    {
      pSrc += 4;
      pDest += 3;
    }
  }

  *pDest = '\0';
  return output;
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
    p++; // the string is utf-8 thus no conflict with scanning chars (which is all us-ascii).
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
  char *retbuf = NULL, *srcbuf = NULL;
  char sep = '\0';
  char *sep_p = NULL;
  int  retbufsize;
  int line_len = 0;
  int srclen;
  int default_iThreshold;
  int iThreshold;         /* how many bytes we can convert from the src */
  int iEffectLen;         /* the maximum length we can convert from the src */
  PRBool bChop = PR_FALSE;

  if (src == NULL)
    return NULL;

  /* make a copy, so don't need touch original buffer */
  srcbuf = nsCRT::strdup(src);
  if (srcbuf == NULL)
    return NULL;
  begin = srcbuf;

  default_iThreshold = iEffectLen = ( maxLineLen - nsCRT::strlen(charset) - 7 ) * 3 / 4;
  iThreshold = default_iThreshold;


  /* allocate enough buffer for conversion, this way it can avoid
     do another memory allocation which is expensive
   */
  retbufsize = nsCRT::strlen(srcbuf) * 3 + kMAX_CSNAME + 8;
  retbuf =  (char *) PR_Malloc(retbufsize);
  if (retbuf == NULL) {  /* Give up if not enough memory */
    PR_Free(srcbuf);
    return NULL;
  }

  *retbuf = '\0';

  // loop for separating encoded words by the separators
  // the input string is utf-8 at this point
  srclen = nsCRT::strlen(srcbuf);

convert_and_encode:

  while (begin < (srcbuf + srclen))
  { /* get block of data between commas (and other separators) */
    char *p, *q;
    char *buf1, *buf2;
    int len, newsize, convlen, retbuflen;
    PRBool non_ascii;

    retbuflen = nsCRT::strlen(retbuf);
    end = NULL;

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
              if (p == NULL)
                break;
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
        q++;  // the string is utf-8 thus no conflict with scanning chars (which is all us-ascii).
      }
    }

    // convert utf-8 to mail charset
    /* get the to_be_converted_buffer's len */
    len = nsCRT::strlen(begin);

    if ( !intlmime_only_ascii_str(begin) )
    {
      // now the input is utf-8, a character may be more than 2 bytes len
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
        *buf1++ = CR;   *buf1++ = LF; *buf1++ = '\t';
        line_len = 0;
        retbuflen += 3;
        *buf1 = '\0';
        bChop = PR_FALSE;
        iThreshold = default_iThreshold;
      }

      // loop for line wrapping: estimate converted/encoded length 
      // and apply conversion (utf-8 to mail charset)

      /* iEffectLen - the max byte-string length of JIS ( converted form S-JIS )
         name - such as "iso-2022-jp", the encoding name, MUST be shorter than 23 bytes
         7    - is the "=?:?:?=" 
       */
      iEffectLen = ( maxLineLen - line_len - nsCRT::strlen(charset) - 7 ) * 3 / 4;
      while ( PR_TRUE ) {
        int iBufLen;    /* converted buffer's length, not BASE64 */
        if ( len > iThreshold )
          len = ResetLen(iThreshold, begin);

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
          return NULL;
        }
        // utf-8 to mail charset conversion (or iso-8859-1 in case of us-ascii).
        PRUnichar *u = NULL;
        nsAutoString fmt; fmt.AssignWithConversion("%s");
        char aChar = begin[len];
        begin[len] = '\0';
        u = nsTextFormatter::smprintf(fmt.GetUnicode(), begin);
        begin[len] = aChar;
        if (NULL == u) {
            PR_FREEIF(srcbuf);
            PR_FREEIF(retbuf);
            return NULL; //error
        }
        nsresult rv = nsMsgI18NSaveAsCharset(TEXT_PLAIN, 
                                             !nsCRT::strcasecmp(charset, "us-ascii") ? "ISO-8859-1" : charset, 
                                             u, &buf1);
        nsTextFormatter::smprintf_free(u);
        if (NS_FAILED(rv) || NULL == buf1) {
          PR_FREEIF(srcbuf);
          PR_FREEIF(retbuf);
          return NULL; //error
        }
        iBufLen = nsCRT::strlen(buf1);
//        buf1 = (char *) cvtfunc(obj, (unsigned char *)begin, len);
//        iBufLen = nsCRT::strlen( buf1 );
        if (iBufLen <= 0) {
          if (NULL == end) {
            PR_FREEIF(srcbuf);
            PR_FREEIF(retbuf);
            return NULL; //error
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
            buf1 = NULL;
          }
        } else {
          end = begin + len - 1;
          break;
        }
      }
      if (bChop && (NULL!=sep_p)) {
        *sep_p = sep;   /* we are length limited so we do not need this */
        sep = '\0';     /* artifical terminator. So, restore the original character */
        sep_p = NULL;
      }

      if (!buf1)
      {
        PR_Free(srcbuf);
        PR_Free(retbuf);
        return NULL;
      }

      // converted to mail charset, now apply MIME encode
      const char *msgHeaderEncoding = intl_message_header_encoding((const char *) charset);
      if (!nsCRT::strcasecmp(msgHeaderEncoding, kMsgHeaderBEncoding))
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

      if (buf2 == NULL) /* QUIT if memory allocation failed */
      {
        PR_Free(srcbuf);
        PR_Free(retbuf);
        return NULL;
      }

      /* realloc memory for retbuff if necessary,
         7: =?...?B?..?=, 3: CR LF TAB */
      convlen = nsCRT::strlen(buf2) + nsCRT::strlen(charset) + 7;
      newsize = convlen + retbuflen + 3 + 2;  /* 2:SEP '\0', 3:CRLFTAB */

      if (newsize > retbufsize)
      {
        char *tempbuf;
        tempbuf = (char *) PR_Realloc(retbuf, newsize);
        if (tempbuf == NULL)  /* QUIT, if not enough memory left */
        {
          PR_Free(buf2);
          PR_Free(srcbuf);
          PR_Free(retbuf);
          return NULL;
        }
        retbuf = tempbuf;
        retbufsize = newsize;
      }
      /* buf1 points to end of current retbuf */
      buf1 = retbuf + retbuflen;

      if ((line_len > 10) &&
          ((line_len + convlen) > maxLineLen))
      {
        *buf1++ = CR;
        *buf1++ = LF;
        *buf1++ = '\t';
        line_len = 0;
        iThreshold = default_iThreshold;
      }
      *buf1 = '\0';

      /* Add encoding tag for base64 and QP */
      PL_strcat(buf1, "=?");
      PL_strcat(buf1, charset );
      if (!nsCRT::strcasecmp(msgHeaderEncoding, kMsgHeaderBEncoding))
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
        if (tempbuf == NULL)
        {
          PR_Free(srcbuf);
          PR_Free(retbuf);
          return NULL;
        }
        retbuf = tempbuf;
        retbufsize = newsize;
      }
      buf1 = retbuf + retbuflen;

      if ((line_len > 10) &&
          ((line_len + len) > maxLineLen))
      {
        *buf1++ = CR;
        *buf1++ = LF;
        *buf1++ = '\t';
        line_len = 0;
        iThreshold = default_iThreshold;
      }
      /* copy buffer from begin to buf1 stripping CRLFTAB */
      for (p = begin; *p; p++)
      {
        if (*p == CR || *p == LF || *p == TAB)
          len --;
        else
          *buf1++ = *p;
      }
      *buf1 = '\0';
      line_len += len + 1;  /* 1: SEP */
    }

    buf1 = buf1 + nsCRT::strlen(buf1);
    if (sep == CR || sep == LF || sep == TAB) /* strip CR,LF,TAB */
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

// input utf-8, return NULL in case of error.
static
char *utf8_EncodeMimePartIIStr(const char *subject, char *charset, int maxLineLen)
{
  int iSrcLen;
  char *buf = NULL;

  if (subject == NULL)
    return NULL;

  iSrcLen = nsCRT::strlen(subject);

  /* check to see if subject are all ascii or not */
  if((*subject == '\0') ||
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


static char *intlmime_decode_qp(char *in)
{
  int i = 0, length;
  char token[3];
  char *out, *dest = 0;

  out = dest = (char *)PR_Malloc(nsCRT::strlen(in)+1);
  if (dest == NULL)
    return NULL;
  memset(out, 0, nsCRT::strlen(in)+1);
  length = nsCRT::strlen(in);
  while (length > 0 || i != 0)
  {
    while (i < 3 && length > 0)
    {
      token [i++] = *in;
      in++;
      length--;
    }

    if (i < 3)
    {
      /* Didn't get enough for a complete token.
         If it might be a token, unread it.
         Otherwise, just dump it.
       */
      strncpy (out, token, i);
      break;
    }
    i = 0;

    if (token [0] == '=')
    {
      unsigned char c = 0;
      if (token[1] >= '0' && token[1] <= '9')
        c = token[1] - '0';
      else if (token[1] >= 'A' && token[1] <= 'F')
        c = token[1] - ('A' - 10);
      else if (token[1] >= 'a' && token[1] <= 'f')
        c = token[1] - ('a' - 10);
      else if (token[1] == CR || token[1] == LF)
      {
        /* =\n means ignore the newline. */
        if (token[1] == CR && token[2] == LF)
          ;               /* swallow all three chars */
        else
        {
          in--;   /* put the third char back */
          length++;
        }
        continue;
      }
      else
      {
        /* = followed by something other than hex or newline -
         pass it through unaltered, I guess. (But, if
         this bogus token happened to occur over a buffer
         boundary, we can't do this, since we don't have
         space for it.  Oh well.  Screw it.)  */
        if (in > out) *out++ = token[0];
        if (in > out) *out++ = token[1];
        if (in > out) *out++ = token[2];
        continue;
      }

      /* Second hex digit */
      c = (c << 4);
      if (token[2] >= '0' && token[2] <= '9')
        c += token[2] - '0';
      else if (token[2] >= 'A' && token[2] <= 'F')
        c += token[2] - ('A' - 10);
      else if (token[2] >= 'a' && token[2] <= 'f')
        c += token[2] - ('a' - 10);
      else
      {
        /* We got =xy where "x" was hex and "y" was not, so
         treat that as a literal "=", x, and y. (But, if
         this bogus token happened to occur over a buffer
         boundary, we can't do this, since we don't have
         space for it.  Oh well.  Screw it.) */
        if (in > out) *out++ = token[0];
        if (in > out) *out++ = token[1];
        if (in > out) *out++ = token[2];
        continue;
      }

      *out++ = (char) c;
    }
    else
    {
      *out++ = token [0];

      token[0] = token[1];
      token[1] = token[2];
      i = 2;
    }
  }
  /* take care of special underscore case */
  for (out = dest; *out; out++)
    if (*out == '_')        *out = ' ';
  return dest;
}

/*
  intlmime_is_mime_part2_header:
*/
static PRBool intlmime_is_mime_part2_header(const char *header)
{
  return ((
           PL_strstr(header, "=?") &&
           (
            PL_strstr(header, "?q?")  ||
            PL_strstr(header, "?Q?")  ||
            PL_strstr(header, "?b?")  ||
            PL_strstr(header, "?B?")
           )
          ) ? PR_TRUE : PR_FALSE );
}

static
char *intl_decode_mime_part2_str(const char *header, char* charset)
{
  char *work_buf = NULL;
  char *output_p = NULL;
  char *retbuff = NULL;
  char *p, *q, *decoded_text;
  char *begin; /* tracking pointer for where we are in the work buffer */
  int  ret = 0;

  // initialize charset name to an empty string
  if (charset)
    charset[0] = '\0';

  mime_SACopy(&work_buf, header);  /* temporary buffer */
  mime_SACopy(&retbuff, header);

  if (work_buf == NULL || retbuff == NULL)
    return NULL;

  output_p = retbuff;
  begin = work_buf;

  while (*begin != '\0')
  {
    char * output_text;

    /*              GetCharset();           */
    p = strstr(begin, "=?");
    if (p == NULL)
      break;                          /* exit the loop because the rest are not encoded */
    *p = '\0';
    /* skip strings don't need conversion */
    strncpy(output_p, begin, p - begin);
    output_p += p - begin;

    p += 2;
    begin = p;

    q = strchr(p, '?');  /* Get charset info */
    if (q == NULL)
            break;                          /* exit the loop because there are no charset info */
    *q++ = '\0';
    if (charset)
      PL_strcpy(charset, nsCRT::strcasecmp(p, "us-ascii") ? p : "ISO-8859-1");

    if (*(q+1) == '?' &&
        (*q == 'Q' || *q == 'q' || *q == 'B' || *q == 'b'))
    {
      p = strstr(q+2, "?=");
      if(p != NULL)
        *p = '\0';
      if(*q == 'Q' || *q == 'q')
        decoded_text = intlmime_decode_qp(q+2);
      else
        decoded_text = intlmime_decode_base64_buf(q+2);
    }
    else
      break;                          /* exit the loop because we don't know the encoding method */

    begin = (p != NULL) ? p + 2 : (q + nsCRT::strlen(q));

    if (decoded_text == NULL)
      break;                          /* exit the loop because we have problem to decode */

    ret = 1;

    output_text = (char *)decoded_text;

    PR_ASSERT(output_text != NULL);
    PL_strcpy(output_p, (char *)output_text);
    output_p += nsCRT::strlen(output_text);

    if (output_text != decoded_text)
      PR_Free(output_text);
    PR_Free(decoded_text);
  }
  PL_strcpy(output_p, (char *)begin);     /* put the tail back  */

  if (work_buf)
    PR_Free(work_buf);

  if (ret)
  {
    return retbuff;
  }
  else
  {
    PR_Free(retbuff);
    PL_strcpy(charset, "us-ascii");       /* charset was not encoded, put us-ascii */
    return nsCRT::strdup(header);             /* nothing to decode */
  }
}

////////////////////////////////////////////////////////////////////////////////

nsIStringCharsetDetector* MimeCharsetConverterClass::mDetector = NULL;
nsCString MimeCharsetConverterClass::mDetectorContractID;

MimeCharsetConverterClass::MimeCharsetConverterClass()
{
  mDecoder = NULL;
  mEncoder = NULL;
  mDecoderDetected = NULL;
  mMaxNumCharsDetect = -1;
  mNumChars = 0;
  mAutoDetect = PR_FALSE;
}

MimeCharsetConverterClass::~MimeCharsetConverterClass()
{
  NS_IF_RELEASE(mDecoder);
  NS_IF_RELEASE(mEncoder);
  NS_IF_RELEASE(mDecoderDetected);
}

PRInt32 MimeCharsetConverterClass::Initialize(const char* from_charset, const char* to_charset, 
                                              const PRBool autoDetect, const PRInt32 maxNumCharsDetect)
{
  nsresult res;

  NS_ASSERTION(NULL == mEncoder, "No reinitialization allowed.");

  mInputCharset.AssignWithConversion(from_charset);     // remember input charset for a hint
  if (mInputCharset.IsEmpty()) {
    mInputCharset.AssignWithConversion("ISO-8859-1");
  }
  mOutputCharset.AssignWithConversion(to_charset);      // remember output charset
  if (mOutputCharset.IsEmpty()) {
    mOutputCharset.AssignWithConversion("UTF-8");
  }
  mAutoDetect = autoDetect;
  mMaxNumCharsDetect = maxNumCharsDetect;

  // Resolve charset alias
  NS_WITH_SERVICE(nsICharsetAlias, calias, kCharsetAliasCID, &res); 
  if (NS_SUCCEEDED(res)) {
    nsString aAlias(mInputCharset);
    if (aAlias.Length()) {
      res = calias->GetPreferred(aAlias, mInputCharset);
      if (NS_FAILED(res)) {
        mInputCharset.AssignWithConversion("ISO-8859-1");
      }
    }
    aAlias = mOutputCharset;
    if (aAlias.Length()) {
      res = calias->GetPreferred(aAlias, mOutputCharset);
      if (NS_FAILED(res)) {
        mOutputCharset.AssignWithConversion("UTF-8");
      }
    }
  }

  if (mAutoDetect) {
    char detector_contractid[128];
    PRUnichar* detector_name = nsnull;
    PL_strcpy(detector_contractid, NS_STRCDETECTOR_CONTRACTID_BASE);

    NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &res); 
    if (NS_SUCCEEDED(res)) {
      if (NS_SUCCEEDED(prefs->CopyUnicharPref("mail.charset.detector", &detector_name))) {
        PL_strcat(detector_contractid, NS_ConvertUCS2toUTF8(detector_name));
        PR_FREEIF(detector_name);
      }
      else if (NS_SUCCEEDED(prefs->GetLocalizedUnicharPref("intl.charset.detector", &detector_name))) {
        PL_strcat(detector_contractid, NS_ConvertUCS2toUTF8(detector_name));
        PR_FREEIF(detector_name);
      }
    }

    if (!mDetectorContractID.Equals(detector_contractid)) {
      // may have multi-thread issue updating these static variables
      // but charset detector can only be changed globally through UI
      NS_IF_RELEASE(mDetector);
      mDetectorContractID.Assign("");
      // create instance when the detector name is not empty
      if (nsCRT::strcmp(detector_contractid, NS_STRCDETECTOR_CONTRACTID_BASE)) {
        res = nsComponentManager::CreateInstance(detector_contractid, nsnull, 
                                                 NS_GET_IID(nsIStringCharsetDetector), (void**)&mDetector);
        if (NS_SUCCEEDED(res)) {
          mDetectorContractID.Assign(detector_contractid);
        }
      }
    }
  }

  // No need to do the conversion then do not create converters. 
  if (!mAutoDetect && !NeedCharsetConversion(mInputCharset, mOutputCharset)) {
    return 0;
  }

  // Set up charset converters.
  NS_WITH_SERVICE(nsICharsetConverterManager, ccm, kCharsetConverterManagerCID, &res); 

  if (NS_SUCCEEDED(res)) {
    // create a decoder (conv to unicode), ok if failed if we do auto detection
    res = ccm->GetUnicodeDecoder(&mInputCharset, &mDecoder);
    if (NS_SUCCEEDED(res) || mAutoDetect) {
      // create an encoder (conv from unicode)
      res = ccm->GetUnicodeEncoder(&mOutputCharset, &mEncoder);
    }
  }

  return NS_SUCCEEDED(res) ? 0 : -1;
}

PRInt32 MimeCharsetConverterClass::Convert(const char* inBuffer, const PRInt32 inLength, 
                                           char** outBuffer, PRInt32* outLength,
                                           PRInt32* numUnConverted)
{
  nsresult res;

  if (NULL != numUnConverted) {
    *numUnConverted = 0;
  }

  // Encoder is not available, duplicate the input.
  if (NULL == mEncoder) {
    *outBuffer = (char *) PR_Malloc(inLength+1);
    if (NULL != *outBuffer) {
      nsCRT::memcpy(*outBuffer, inBuffer, inLength);
      *outLength = inLength;
      (*outBuffer)[inLength] = '\0';
      return 0;
    }
    return -1;
  }

  nsIUnicodeDecoder* decoder = mDecoder;
  nsIUnicodeEncoder* encoder = mEncoder;

  // try auto detection for this string
  if (mAutoDetect && NULL != mDetector && (mMaxNumCharsDetect == -1 || mMaxNumCharsDetect > mNumChars)) {
    nsString aCharsetDetected;
    const char* oCharset;
    nsDetectionConfident oConfident;
    res = mDetector->DoIt(inBuffer, inLength, &oCharset, oConfident);
    if (NS_SUCCEEDED(res) && (eBestAnswer == oConfident || eSureAnswer == oConfident)) {
      aCharsetDetected.AssignWithConversion(oCharset);

      // Check if need a conversion.
      if (!NeedCharsetConversion(aCharsetDetected, mOutputCharset)) {
        *outBuffer = (char *) PR_Malloc(inLength+1);
        if (NULL != *outBuffer) {
          nsCRT::memcpy(*outBuffer, inBuffer, inLength);
          *outLength = inLength;
          (*outBuffer)[inLength] = '\0';
          return 0;
        }
        return -1;
      }
      else {
        NS_WITH_SERVICE(nsICharsetConverterManager, ccm, kCharsetConverterManagerCID, &res); 
        if (NS_SUCCEEDED(res)) {
          NS_IF_RELEASE(mDecoderDetected);
          mDecoderDetected = nsnull;
          res = ccm->GetUnicodeDecoder(&aCharsetDetected, &mDecoderDetected);
          if (NS_SUCCEEDED(res)) {
            decoder = mDecoderDetected;   // use detected charset instead
          }
        }
      }
    }
  }

  // update the total so far
  mNumChars += inLength;

  // Decoders are not available, do fallback
  if (NULL == mDecoder && NULL == mDecoderDetected) {
    *outBuffer = (char *) PR_Malloc(inLength+1);
    if (NULL != *outBuffer) {
      nsCRT::memcpy(*outBuffer, inBuffer, inLength);
      *outLength = inLength;
      (*outBuffer)[inLength] = '\0';
      return 0;
    }
    return -1;
  }

  // do the conversion
  PRUnichar *unichars;
  PRInt32 unicharLength;
  PRInt32 srcLen = inLength;
  PRInt32 dstLength = 0;
  char *dstPtr;

  res = decoder->GetMaxLength(inBuffer, srcLen, &unicharLength);
  // allocate temporary buffer to hold unicode string
  unichars = new PRUnichar[unicharLength];
  if (unichars == nsnull) {
    res = NS_ERROR_OUT_OF_MEMORY;
  }
  else {
    // convert to unicode
    res = decoder->Convert(inBuffer, &srcLen, unichars, &unicharLength);
    if (NS_SUCCEEDED(res)) {
      res = encoder->GetMaxLength(unichars, unicharLength, &dstLength);
      // allocale an output buffer
      dstPtr = (char *) PR_Malloc(dstLength + 1);
      if (dstPtr == nsnull) {
        res = NS_ERROR_OUT_OF_MEMORY;
      }
      else {
        PRInt32 buffLength = dstLength;
        // convert from unicode
        res = encoder->SetOutputErrorBehavior(nsIUnicodeEncoder::kOnError_Replace, nsnull, '?');
        if (NS_SUCCEEDED(res)) {
          res = encoder->Convert(unichars, &unicharLength, dstPtr, &dstLength);
          if (NS_SUCCEEDED(res)) {
            PRInt32 finLen = buffLength - dstLength;
            res = encoder->Finish((char *)(dstPtr+dstLength), &finLen);
            if (NS_SUCCEEDED(res)) {
              dstLength += finLen;
            }
            dstPtr[dstLength] = '\0';
            *outBuffer = dstPtr;       // set the result string
            *outLength = dstLength;
          }
        }
      }
    }
    delete [] unichars;
  }

  return NS_SUCCEEDED(res) ? 0 : -1;
}

PRBool MimeCharsetConverterClass::NeedCharsetConversion(const nsString& from_charset, const nsString& to_charset)
{
  if (from_charset.Length() == 0 || to_charset.Length() == 0) 
    return PR_FALSE;
  else if (from_charset.EqualsIgnoreCase(to_charset)) {
    return PR_FALSE;
  }
  else if ((from_charset.EqualsIgnoreCase("us-ascii") && to_charset.EqualsIgnoreCase("UTF-8")) ||
      (from_charset.EqualsIgnoreCase("UTF-8") && to_charset.EqualsIgnoreCase("us-ascii"))) {
    return PR_FALSE;
  }
  return PR_TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// BEGIN PUBLIC INTERFACE
extern "C" {
#define PUBLIC
PUBLIC char *INTL_DecodeMimePartIIStr(const char *header, PRInt16 wincsid, PRBool dontConvert)
{
// Obsolescent
  return nsCRT::strdup(header);
}
PUBLIC char *INTL_EncodeMimePartIIStr(char *subject, PRInt16 wincsid, PRBool bUseMime)
{
// Obsolescent
  return nsCRT::strdup(subject);
}
/*  This is a routine used to re-encode subject lines for use in the summary file.
    The reason why we specify a different length here is because we are not encoding
    the string for use in a mail message, but rather want to stuff as much content
    into the subject string as possible. */
PUBLIC char *INTL_EncodeMimePartIIStr_VarLen(char *subject, PRInt16 wincsid, PRBool bUseMime, int encodedWordSize)
{
// Obsolescent
  return nsCRT::strdup(subject);
}

PRInt32 MIME_ConvertString(const char* from_charset, const char* to_charset,
                           const char* inCstring, char** outCstring)
{
  PRInt32 outLength;
  return MIME_ConvertCharset(PR_FALSE, from_charset, to_charset, 
                             inCstring, nsCRT::strlen(inCstring), outCstring, &outLength, NULL);
}

PRInt32 MIME_ConvertCharset(const PRBool autoDetection, const char* from_charset, const char* to_charset,
                            const char* inBuffer, const PRInt32 inLength, char** outBuffer, PRInt32* outLength,
                            PRInt32* numUnConverted)
{
  MimeCharsetConverterClass aMimeCharsetConverterClass;
  PRInt32 res;

  res = aMimeCharsetConverterClass.Initialize(from_charset, to_charset, autoDetection, -1);

  if (res != -1) {
    res = aMimeCharsetConverterClass.Convert(inBuffer, inLength, outBuffer, outLength, NULL);
  }

  return res;
}

extern "C" char *MIME_DecodeMimePartIIStr(const char *header, char *charset,
                                          PRBool eatContinuations)
{
  char *result = nsnull;

  if (header == 0)
    return nsnull;

  // If no MIME encoded then do nothing otherwise decode the input.
  if (*header != '\0' && intlmime_is_mime_part2_header(header)) {
	  result = intl_decode_mime_part2_str(header, charset);
	  if (eatContinuations)
		  result = MIME_StripContinuations(result);
  }
  else if (*charset == '\0') {
    // no charset name is specified then assume it's us-ascii (or ISO-8859-1 if 8bit) 
    // and dup the input (later change the caller to avoid the duplication)
    unsigned char *cp = (unsigned char *) header;
    PL_strcpy(charset, "us-ascii");
    while (*cp) {
      if (*cp > 127) {
        PL_strcpy(charset, "ISO-8859-1");
        break;
      }
      cp++;
    }
    return nsCRT::strdup(header); 
  }

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

// Destructor called when unloading the DLL
void comi18n_destructor()
{
  NS_IF_RELEASE(MimeCharsetConverterClass::mDetector);
}

} /* end of extern "C" */
// END PUBLIC INTERFACE

/*
main()
{
        char *encoded, *decoded;
        printf("mime\n");
        encoded = intl_EncodeMimePartIIStr("hello world…", INTL_CsidToCharsetNamePt(0), PR_TRUE,
kMIME_ENCODED_WORD_SIZE);
        printf("%s\n", encoded);
        decoded = intl_DecodeMimePartIIStr((const char *) encoded,
nsCRT::strlen(encoded), PR_TRUE);

        return 0;
}
*/

