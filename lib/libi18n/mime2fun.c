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
/*	mime2fun.c	*/
/*  Function related to MIME-2 support	*/

#include "intlpriv.h"
#include "prefapi.h"

/*
	"The character set names may be up to 40 characters taken from the
	printable characters of US-ASCII.  However, no distinction is made
	between use of upper and lower case letters." [RFC 1700]
*/
typedef struct {
	int16	csid_key;
	int16	csid_target;
} cs_csid_map_t;


	/* Maps a window encoding to the MIME encoding used for posting
	 * news and internet email.  String is for the MIME charset string.

		Currently, this table is only used for HEADER ENCODING!!!!
		It is not used for HEADER decoding!!!!
		It is not used for Body Decoding or Encoding!!!!
		NOTE: We need to change this  from win_csid base to doc_csid base
	
	 */

PRIVATE cs_csid_map_t	cs_mime_csidmap_tbl[] = {
			{CS_ASCII,		CS_ASCII	},
			
			{CS_LATIN1,		CS_LATIN1	},
			
			{CS_JIS,		CS_JIS		},
			{CS_SJIS,		CS_JIS		},
			{CS_EUCJP,		CS_JIS,		},
			
			{CS_KSC_8BIT,	CS_KSC_8BIT	},
				
			{CS_GB_8BIT,	CS_GB_8BIT	},
			
			{CS_BIG5,		CS_BIG5		},
			{CS_CNS_8BIT,	CS_BIG5		},
			
			{CS_MAC_ROMAN,	CS_LATIN1	},
			
			{CS_LATIN2,		CS_LATIN2	},
			{CS_MAC_CE,		CS_LATIN2	},
			{CS_CP_1250,	CS_LATIN2	},

			{CS_8859_5,			CS_KOI8_R	},
			{CS_KOI8_R,			CS_KOI8_R	},
			{CS_MAC_CYRILLIC,	CS_KOI8_R	},
			{CS_CP_1251,		CS_KOI8_R	},
			
			{CS_8859_7,		CS_8859_7	},
			{CS_CP_1253,		CS_8859_7	},
			{CS_MAC_GREEK,	CS_8859_7	},

			{CS_8859_9,		CS_8859_9	},
			{CS_MAC_TURKISH,CS_8859_9	},
			
			{CS_UTF8,		CS_UTF8		},
			{CS_UTF7,		CS_UTF7		},
			{CS_UCS2,		CS_UTF7		},
			{CS_UCS2_SWAP,	CS_UTF7		},
			{0,				0,			}
};

#define MAXLINELEN 72
#define IS_MAIL_SEPARATOR(p) ((*(p) == ',' || *(p) == ' ' || *(p) == '\"' || *(p) == ':' || \
  *(p) == '(' || *(p) == ')' || *(p) == '\\' || (unsigned char)*p < 0x20))

/*
	Prototype for Private Function
*/
PRIVATE void    intlmime_init_csidmap();
PRIVATE XP_Bool	intlmime_only_ascii_str(const char *s);
PRIVATE char *	intlmime_encode_mail_address(int wincsid, const char *src, CCCDataObject obj, 
											int maxLineLen);
PRIVATE char *	intlmime_encode_next8bitword(int wincsid, char *src);
PRIVATE int16	intlmime_get_outgoing_mime_csid(int16);
PRIVATE int16	intlmime_map_csid(cs_csid_map_t *csmimep, int16	csid_key);

/*	we should consider replace this base64 decodeing and encoding function with a better one */
PRIVATE int		intlmime_decode_base64 (const char *in, char *out);
PRIVATE char *	intlmime_decode_qp(char *in);
PRIVATE int		intlmime_encode_base64 (const char *in, char *out);
PRIVATE char *	intlmime_decode_base64_buf(char *subject);
PRIVATE char *	intlmime_encode_base64_buf(char *subject, size_t size);
PRIVATE char *	intlmime_encode_qp_buf(char *subject);


PRIVATE XP_Bool intlmime_is_hz(const char *header);
PRIVATE XP_Bool intlmime_is_mime_part2_header(const char *header);
PRIVATE XP_Bool intlmime_is_iso_2022_xxx(const char *, int16 );
PRIVATE	char *	intl_decode_mime_part2_str(const char *, int , XP_Bool );
PRIVATE	char *	intl_DecodeMimePartIIStr(const char *, int16 , XP_Bool );
PRIVATE char *	intl_EncodeMimePartIIStr(char *subject, int16 wincsid, XP_Bool bUseMime, int maxLineLen);


#if 0
/* Prototype of callback routine invoked by prefapi when the pref value changes 
 * Win16 build fails if this is declared as static.
 */
MODULE_PRIVATE
int PR_CALLBACK intlmime_get_mail_strictly_mime(const char * newpref, void * data);
#endif

/*	We probably should change these into private instead of PUBLIC */
PUBLIC char *DecodeBase64Buffer(char *subject);
PUBLIC char *EncodeBase64Buffer(char *subject, size_t size);


/* 4.0: Made Encode & Decode public for use by libpref; added size param.
 */
PUBLIC char *EncodeBase64Buffer(char *subject, size_t size)
{
	/* This function should be obsolete */
	/* We should not make this public in libi18n */
	/* We should use the new Base64 Encoder wrote by jwz in libmime */
	return intlmime_encode_base64_buf(subject, size);
}

PUBLIC char *DecodeBase64Buffer(char *subject)
{
	/* This function should be obsolete */
	/* We should not make this public in libi18n */
	/* We should use the new Base64 Decoder wrote by jwz in libmime */
	return intlmime_decode_base64_buf(subject);
}

#if defined(MOZ_MAIL_COMPOSE) || defined(MOZ_MAIL_NEWS)

/*
	Implementation 
*/
/*
	INTL_DefaultMailCharSetID,
*/
PUBLIC int16 INTL_DefaultMailCharSetID(int16 csid)
{
	int16 retcsid;
 	csid &= ~CS_AUTO;

	intlmime_init_csidmap();
	retcsid = intlmime_map_csid(cs_mime_csidmap_tbl, csid);

	if(retcsid == CS_KSC_8BIT)
		retcsid = CS_2022_KR;
	return retcsid;
}

PUBLIC int16 INTL_DefaultNewsCharSetID(int16 csid)
{

	if (csid == 0)
		csid = INTL_DefaultDocCharSetID(0);
	csid &= ~CS_AUTO;
	intlmime_init_csidmap();
	return intlmime_map_csid(cs_mime_csidmap_tbl, csid);
}

PUBLIC
char *INTL_DecodeMimePartIIStr(const char *header, int16 wincsid, XP_Bool dontConvert)
{
	return intl_DecodeMimePartIIStr(header, wincsid, dontConvert);
}
PUBLIC
char *INTL_EncodeMimePartIIStr(char *subject, int16 wincsid, XP_Bool bUseMime)
{
	return intl_EncodeMimePartIIStr(subject, wincsid, bUseMime, MAXLINELEN);
}
#endif /* MOZ_MAIL_COMPOSE || MOZ_MAIL_NEWS */

#ifdef MOZ_MAIL_NEWS
/*  This is a routine used to re-encode subject lines for use in the summary file.
	The reason why we specify a different length here is because we are not encoding
	the string for use in a mail message, but rather want to stuff as much content
	into the subject string as possible. */
PUBLIC
char *INTL_EncodeMimePartIIStr_VarLen(char *subject, int16 wincsid, XP_Bool bUseMime, int encodedWordSize)
{
	return intl_EncodeMimePartIIStr(subject, wincsid, bUseMime, encodedWordSize);
}
#endif /* MOZ_MAIL_NEWS */


#if defined(MOZ_MAIL_COMPOSE) || defined(MOZ_MAIL_NEWS)
/*	some utility function used by this file */
PRIVATE XP_Bool intlmime_only_ascii_str(const char *s)
{
	for(; *s; s++)
		if(*s & 0x80)
			return FALSE;
	return TRUE;
}
#endif /* MOZ_MAIL_COMPOSE || MOZ_MAIL_NEWS */

#ifdef MOZ_MAIL_NEWS
PRIVATE void intlmime_update_csidmap(int16 csid_key, int16 csid_target)
{
	cs_csid_map_t * mapp;
	
	for(mapp = cs_mime_csidmap_tbl ; 
		mapp->csid_key != 0 ; 
		mapp++) 
		{
		if (mapp->csid_key == csid_key) 
			mapp->csid_target = csid_target;
		}
}

#if 0
/* callback routine invoked by prefapi when the pref value changes */
MODULE_PRIVATE
int PR_CALLBACK intlmime_get_mail_strictly_mime(const char * newpref, void * data)
{
	XP_Bool	mail_strictly_mime = FALSE;

	if (PREF_NOERROR == PREF_GetBoolPref("mail.strictly_mime", &mail_strictly_mime))
	{
		intlmime_update_csidmap(CS_UTF8, mail_strictly_mime ? CS_UTF7 : CS_UTF8);
		intlmime_update_csidmap(CS_UCS2, mail_strictly_mime ? CS_UTF7 : CS_UTF8);
		intlmime_update_csidmap(CS_UCS2_SWAP, mail_strictly_mime ? CS_UTF7 : CS_UTF8);
	}
	
	return PREF_NOERROR;
}
#endif
#endif /* MOZ_MAIL_NEWS */

#if defined(MOZ_MAIL_COMPOSE) || defined(MOZ_MAIL_NEWS)
PRIVATE void
intlmime_init_csidmap()
{
	static XP_Bool initialized = FALSE;

	if(initialized)
		return;

#if 0 /* UTF-8 is sent as UTF-8 regardless of the pref setting. */
        {
	  XP_Bool	mail_strictly_mime;
	  /* modify csidmap for UTF-8 if the pref is not strictly mime */
	  if (PREF_NOERROR == PREF_GetBoolPref("mail.strictly_mime", &mail_strictly_mime))
  	  {
		intlmime_update_csidmap(CS_UTF8, mail_strictly_mime ? CS_UTF7 : CS_UTF8);
		intlmime_update_csidmap(CS_UCS2, mail_strictly_mime ? CS_UTF7 : CS_UTF8);
		intlmime_update_csidmap(CS_UCS2_SWAP, mail_strictly_mime ? CS_UTF7 : CS_UTF8);
		/* to detect pref change */
		PREF_RegisterCallback("mail.strictly_mime", intlmime_get_mail_strictly_mime, NULL);
	  }
        }
#endif
		
    /* Speical Hack for Cyrllic */
    /* We need to know wheater we should send KOI8-R or ISO-8859-5 */

	{
		cs_csid_map_t * mapp;
		static const char* pref_mailcharset_cyrillic = "intl.mailcharset.cyrillic";
		char *mailcharset_cyrillic = NULL;
		int16 mailcsid_cyrillic = CS_UNKNOWN;

		if( PREF_NOERROR == PREF_CopyCharPref(pref_mailcharset_cyrillic, 
								&mailcharset_cyrillic))
		{
			mailcsid_cyrillic = INTL_CharSetNameToID(mailcharset_cyrillic);
			XP_FREE(mailcharset_cyrillic);

			if(CS_UNKNOWN != mailcsid_cyrillic)
			{
				for(mapp = cs_mime_csidmap_tbl ; 
					mapp->csid_key != 0 ; 
					mapp++) 
				{
					if (
						(mapp->csid_key == CS_KOI8_R) ||  
						(mapp->csid_key == CS_8859_5) ||
						(mapp->csid_key == CS_MAC_CYRILLIC) || 
						(mapp->csid_key == CS_CP_1251) 
			  		 ) 
					{
						mapp->csid_target = mailcsid_cyrillic;
					}
				}
			}
		}
	}

	initialized = TRUE;
}

PRIVATE int16
intlmime_map_csid(cs_csid_map_t *mapp, int16	csid_key)
{
	for( ; mapp->csid_key != 0 ; mapp++) 
	{
		if (csid_key == mapp->csid_key) 
			return(mapp->csid_target);
	}
	return(csid_key);			/* causes no conversion */
}

PRIVATE int16
intlmime_get_outgoing_mime_csid(int16	win_csid)
{
 	win_csid &= ~CS_AUTO;
	intlmime_init_csidmap();
	return intlmime_map_csid(cs_mime_csidmap_tbl, win_csid);
}

PRIVATE char *intlmime_decode_qp(char *in)
{
	int i = 0, length;
	char token[3];
	char *out, *dest = 0;

	out = dest = (char *)XP_ALLOC(strlen(in)+1);
	if (dest == NULL)
		return NULL;
	memset(out, 0, strlen(in)+1);
	length = strlen(in);
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
					;		/* swallow all three chars */
			  	else
				{
				  	in--;	/* put the third char back */
				  	length++;
				}
			  	continue;
			}
		  	else
			{
			  	/* = followed by something other than hex or newline -
				 pass it through unaltered, I guess.  (But, if
				 this bogus token happened to occur over a buffer
				 boundary, we can't do this, since we don't have
				 space for it.  Oh well.  Forget it.)  */
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
				 treat that as a literal "=", x, and y.  (But, if
				 this bogus token happened to occur over a buffer
				 boundary, we can't do this, since we don't have
				 space for it.  Oh well.  Forget it.) */
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
		if (*out == '_') 	*out = ' ';
	return dest;
}

#endif  /* MOZ_MAIL_COMPOSE || MOZ_MAIL_NEWS */

PRIVATE int intlmime_decode_base64 (const char *in, char *out)
{
  /* reads 4, writes 3. */
  int j;
  unsigned long num = 0;

  for (j = 0; j < 4; j++)
	{
	  unsigned char c;
	  if (in[j] >= 'A' && in[j] <= 'Z')		 c = in[j] - 'A';
	  else if (in[j] >= 'a' && in[j] <= 'z') c = in[j] - ('a' - 26);
	  else if (in[j] >= '0' && in[j] <= '9') c = in[j] - ('0' - 52);
	  else if (in[j] == '+')				 c = 62;
	  else if (in[j] == '/')				 c = 63;
	  else if (in[j] == '=')				 c = 0;
	  else
	  {
		/*	abort ();	*/
		strcpy(out, in);	/* I hate abort */
		return 0;
	  }
	  num = (num << 6) | c;
	}

  *out++ = (unsigned char) (num >> 16);
  *out++ = (unsigned char) ((num >> 8) & 0xFF);
  *out++ = (unsigned char) (num & 0xFF);
  return 1;
}

PRIVATE char *intlmime_decode_base64_buf(char *subject)
{
	char *output = 0;
	char *pSrc, *pDest ;
	int i ;

	StrAllocCopy(output, subject); /* Assume converted text are always less than source text */

	pSrc = subject;
	pDest = output ;
	for (i = strlen(subject); i > 3; i -= 4)
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

#if defined(MOZ_MAIL_COMPOSE) || defined(MOZ_MAIL_NEWS)

PRIVATE char *intlmime_convert_but_no_decode(const char *header, int16 mailcsid, int16 wincsid)
{
	char* tmpbuf = NULL, *convbuf = NULL;
    CCCDataObject	obj;
	CCCFunc cvtfunc;
	/* Copy buf to tmpbuf, this guarantee the convresion won't overwrite the origional buffer and  */
	/* It will always return something it any conversion occcur */
	StrAllocCopy(tmpbuf, header);
	
	if(tmpbuf == NULL)
		return NULL;
	
    obj = INTL_CreateCharCodeConverter();
	if (obj == NULL)
		return NULL;
	INTL_GetCharCodeConverter(mailcsid, wincsid, obj);
	convbuf = NULL;
	cvtfunc = INTL_GetCCCCvtfunc(obj);
	if (cvtfunc)
		convbuf = (char*)cvtfunc(obj, (unsigned char*)tmpbuf, (int32)XP_STRLEN((char*)tmpbuf));
	XP_FREE(obj);
	
	/*	if the conversion which use the origional buffer 
		them we return the tmpbuf */
	if(convbuf == NULL)	
		return tmpbuf;

	/*  if the conversion return a different buffer, we free the
		origional one and return the one return from conversion */
	if(convbuf != tmpbuf)
		XP_FREE(tmpbuf);
	return convbuf;
}
/* 
	intlmime_is_hz: it is CS_HZ 
*/
PRIVATE XP_Bool intlmime_is_hz(const char *header)
{
	return (XP_STRSTR(header, "~{") ? TRUE : FALSE);
}
/* 
	intlmime_is_iso_2022_xxx: it is statefule encoding with esc 
*/
PRIVATE XP_Bool intlmime_is_iso_2022_xxx(const char *header, int16 mailcsid)
{
	return (((mailcsid & STATEFUL) && (XP_STRCHR(header, '\033'))) ? TRUE : FALSE);
}
/* 
	intlmime_is_mime_part2_header: 
*/
PRIVATE XP_Bool intlmime_is_mime_part2_header(const char *header)
{
	return ((
		 	 XP_STRSTR(header, "=?") &&
			 ( 
			  XP_STRSTR(header, "?q?")  ||
			  XP_STRSTR(header, "?Q?")  ||
			  XP_STRSTR(header, "?b?")  ||
			  XP_STRSTR(header, "?B?")  
			 )
		    ) ? TRUE : FALSE );	
}

extern char *strip_continuations(char *original);

PRIVATE
char *intl_decode_mime_part2_str(const char *header, int wincsid, XP_Bool dontConvert)
{
	char *work_buf = NULL;
	char *output_p = NULL;
	char *retbuff = NULL;
	char *p, *q, *decoded_text;
	char *begin; /* tracking pointer for where we are in the work buffer */
	int16	 csid = 0;
	int  ret = 0;


	StrAllocCopy(work_buf, header);  /* temporary buffer */
	StrAllocCopy(retbuff, header);

	if (work_buf == NULL || retbuff == NULL)
		return NULL;

	output_p = retbuff;
	begin = work_buf;

	while (*begin != '\0')
	{		
		char * output_text;

		/*		GetCharset();		*/
		p = strstr(begin, "=?");
		if (p == NULL)
			break;				/* exit the loop because the rest are not encoded */
		*p = '\0';
		/* skip strings don't need conversion */
		strncpy(output_p, begin, p - begin);
		output_p += p - begin;

		p += 2;
		begin = p;

		q = strchr(p, '?');  /* Get charset info */
		if (q == NULL)
			break;				/* exit the loop because there are no charset info */
		*q++ = '\0';
		csid = INTL_CharSetNameToID(p);
		if (csid == CS_UNKNOWN)
		{
			/*
			 * @@@ may want to use context's default doc_csid in the future
			 */
			break;				/* exit the loop because we don't know the charset */
		}

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
			break;				/* exit the loop because we don't know the encoding method */

		begin = (p != NULL) ? p + 2 : (q + strlen(q));

		if (decoded_text == NULL) 
			break;				/* exit the loop because we have problem to decode */

		ret = 1;
		if ((! dontConvert) && (csid != wincsid))
			output_text = (char *)intlmime_convert_but_no_decode(decoded_text, csid, (int16)wincsid);
		else
			output_text = (char *)decoded_text;

		XP_ASSERT(output_text != NULL);
		XP_STRCPY(output_p, (char *)output_text);
		output_p += strlen(output_text);

		if (output_text != decoded_text)
			XP_FREE(output_text);
		XP_FREE(decoded_text);
	}
	XP_STRCPY(output_p, (char *)begin);	/* put the tail back  */

	if (work_buf)
		XP_FREE(work_buf);

	if (ret)
	{
		return retbuff;
	}
	else
	{
		XP_FREE(retbuff);
		return NULL;  /* null means no conversion */
	}
}

/* PRIVATE */
/* char* intl_strip_crlftab(char* str) */
/* { */
/* 	char* out, *in; */
/* 	if(str) { */
/* 		for(out = in = str; *in != NULL; in++) */
/* 			if((*in != CR) && (*in != LF) && (*in != TAB)) */
/* 				*out++ = *in; */
/* 		*out = NULL; */
/* 	} */
/* 	return str; */
/* } */


/* 
 IntlDecodeMimePartIIStr
  This functions converts mail charset to Window charset for subject
	Syntax:   =?MimeCharset?[B|Q]?text?=
		MimeCharset = ISO-2022-JP
		              ISO-8859-1
					  ISO-8859-?
					  ....
		?B? :  Base64 Encoding          (used for multibyte encoding)
		?Q? :  Quote Printable Encoding (used for single byte encoding)

  	eg. for Japanese mail, it looks like
    	 =?ISO-2022-JP?B?   ........   ?=
*/
/* IMPORTANT NOTE: */
/* Return NULL in this interface only mean ther are no conversion */
/* It does not mean the conversion is store in the origional buffer */
/* and the length is not change. This is differ from other conversion routine */


PRIVATE
char *intl_DecodeMimePartIIStr(const char *header, int16 wincsid, XP_Bool dontConvert)
{
	int16 mailcsid = INTL_DefaultMailCharSetID(wincsid);
	XP_Bool no8bitdata = TRUE;

	if (header == 0 || *header == '\0')
		return NULL;
	if (wincsid == 0) /* Use global if undefined */
		wincsid = INTL_DefaultWinCharSetID(0);

	no8bitdata = intlmime_only_ascii_str(header);

	/*	Start Special Case Handling		*/
	if(! dontConvert) 
	{
		/* Need to do conversion in here if necessary */
		if(! no8bitdata)
		{
			/*	Special Case 1: 8 Bit */
			/* then we assume it is not mime part 2  encoding, we convert from the internet encoding to wincsid */
			if(wincsid == CS_UTF8)
				return strip_continuations(intlmime_convert_but_no_decode(header, CS_UTF8, (int16)wincsid));
			else
				return strip_continuations(intlmime_convert_but_no_decode(header, mailcsid, (int16)wincsid));
		}
		else
		{
			/* 7bit- It could be MIME Part 2 Header */
			if ((wincsid == CS_GB_8BIT) && (intlmime_is_hz(header)) )
			{  
				/*	Special Case 2: HZ */
				/*	for subject list pane, if it's GB, we only do HZ conversion */
				return strip_continuations(intlmime_convert_but_no_decode(header, CS_GB_8BIT, CS_GB_8BIT));
			}
			else if((wincsid == CS_UTF8) && 
					(! intlmime_is_mime_part2_header(header)))
			{	
				/*	Special Case 3: UTF8, no mime2 */
				return strip_continuations(intlmime_convert_but_no_decode(header, CS_UTF8, CS_UTF8));
			} 
			else if(intlmime_is_iso_2022_xxx(header, mailcsid) && 
					(! intlmime_is_mime_part2_header(header)))
			{
				return strip_continuations(intlmime_convert_but_no_decode(header, mailcsid, wincsid));
			}
		}
	}
	/* Handle only Mime Part 2 after this point */
	return strip_continuations(intl_decode_mime_part2_str(header, wincsid, dontConvert));
}
#endif /* MOZ_MAIL_COMPOSE || MOZ_MAIL_NEWS */



PRIVATE char basis_64[] =
   "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

PRIVATE int intlmime_encode_base64 (const char *in, char *out)
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

PRIVATE char *intlmime_encode_base64_buf(char *subject, size_t size)
{
	char *output = 0;
	char *pSrc, *pDest ;
	int i ;

	output = (char *)XP_ALLOC(size * 4 / 3 + 4);
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

#if defined(MOZ_MAIL_COMPOSE) || defined(MOZ_MAIL_NEWS)
PRIVATE char *intlmime_encode_qp_buf(char *subject)
{
	char *output = 0;
	unsigned char *p, *pDest ;
	int i, n, len ;

	if (subject == NULL || *subject == '\0')
		return NULL;
	len = strlen(subject);
	output = XP_ALLOC(len * 3 + 1);
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

			n = *p & 0x0F;			/* low byte */
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

PRIVATE char *intlmime_encode_next8bitword(int wincsid, char *src)
{
	char *p;
	XP_Bool non_ascii = FALSE;
	if (src == NULL)
		return NULL;
	p = src;
	while (*p == ' ')
		p ++ ;
	while ( *p )
	{
		if ((unsigned char) *p > 0x7F)
			non_ascii = TRUE;
		if ( IS_MAIL_SEPARATOR(p) )
		{
			break;
		}
		p = INTL_NextChar(wincsid, p);
	}

	if (non_ascii)
		return p;
	else
		return NULL;
}

/*
lock then length of input buffer, so the return value is less than iThreshold bytes
*/
PRIVATE int ResetLen( int iThreshold, const char* buffer, int16 wincsid )
{
	const char *begin, *end, *tmp;

	tmp = begin = end = buffer;
	XP_ASSERT( iThreshold > 1 );
	XP_ASSERT( buffer != NULL );
	while( ( end - begin ) <= iThreshold ){
		tmp = end;
		if (!(*end))
			break;
		end = INTL_NextChar( wincsid, (char*)end );
	}

	XP_ASSERT( tmp > begin );
	return tmp - begin;
}

PRIVATE char * intlmime_encode_mail_address(int wincsid, const char *src, CCCDataObject obj,
											int maxLineLen)
{
	char *begin, *end;
	char *retbuf = NULL, *srcbuf = NULL;
	char sep = '\0';
	char *sep_p = NULL;
	char *name;
	int  retbufsize;
	int line_len = 0;
	int srclen;
	int default_iThreshold;
	int iThreshold;		/* how many bytes we can convert from the src */
	int iEffectLen;		/* the maximum length we can convert from the src */
	XP_Bool bChop = FALSE;
	XP_Bool is_being_used_in_email_summary_file = (maxLineLen > 120);
	CCCFunc cvtfunc = NULL;

	if (obj)
		cvtfunc = INTL_GetCCCCvtfunc(obj);

	if (src == NULL || *src == '\0')
		return NULL;
	/* make a copy, so don't need touch original buffer */
	StrAllocCopy(srcbuf, src);
	if (srcbuf == NULL)
		return NULL;
	begin = srcbuf;
	
	name = (char *)INTL_CsidToCharsetNamePt(intlmime_get_outgoing_mime_csid ((int16)wincsid));
	default_iThreshold = iEffectLen = ( maxLineLen - XP_STRLEN( name ) - 7 ) * 3 / 4;
	iThreshold = default_iThreshold;


	/* allocate enough buffer for conversion, this way it can avoid
	   do another memory allocation which is expensive
	 */
	   
	retbufsize = XP_STRLEN(srcbuf) * 3 + MAX_CSNAME + 8;
	retbuf =  XP_ALLOC(retbufsize);
	if (retbuf == NULL)  /* Give up if not enough memory */
	{
		XP_FREE(srcbuf);
		return NULL;
	}

	*retbuf = '\0';

	srclen = XP_STRLEN(srcbuf);
	while (begin < (srcbuf + srclen))
	{	/* get block of data between commas */
		char *p, *q;
		char *buf1, *buf2;
		int len, newsize, convlen, retbuflen;
		XP_Bool non_ascii;

		retbuflen = XP_STRLEN(retbuf);
		end = NULL;
		if (is_being_used_in_email_summary_file) {
		} else {
		/* scan for separator, conversion happens on 8bit
		   word between separators
		 */
		if (IS_MAIL_SEPARATOR(begin))
		{   /*  skip white spaces and separator */
			q = begin;
			while (	IS_MAIL_SEPARATOR(q) )
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
			non_ascii = FALSE;
			for (q = begin; *q;)
			{
				if ((unsigned char) *q > 0x7F)
					non_ascii = TRUE;
				if ( IS_MAIL_SEPARATOR(q) )
				{
					if ((*q == ' ') && (non_ascii == TRUE))
					{
						while ((p = intlmime_encode_next8bitword(wincsid, q)) != NULL)
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
				q = INTL_NextChar(wincsid, q);
			}
		}
		}

		/* get the to_be_converted_buffer's len */
		len = XP_STRLEN(begin);

		if ( !intlmime_only_ascii_str(begin) )
		{
			if (obj && cvtfunc)
			{
				/*
				   	the 30 lenght is calculated as follows (I think)
					total:             30 = 7 + 11 + 8 + 4
					--------------------------------------
					Mime Part II tags: 7  = "=?...?B?...?="
					Charset name:      11 = "iso-2022-jp"
					JIS excape seq.    8  = "<ESC>$B" + "<ESC>(B" * 4/3
					space for one char 4  = 2 * 4/3 rounded up to nearest 4
					Brian Stell 10/97
				*/
				if( ( maxLineLen - line_len < 30 ) || bChop ){
					/* chop first, then continue */
					buf1 = retbuf + retbuflen; 
	  				*buf1++ = CR;	*buf1++ = LF;	*buf1++ = '\t';
					line_len = 0;
					retbuflen += 3;
					*buf1 = '\0';
					bChop = FALSE;
					iThreshold = default_iThreshold;
				}
				/*	iEffectLen - the max byte-string length of JIS ( converted form S-JIS )
					name - such as "iso-2022-jp", the encoding name, MUST be shorter than 23 bytes
					7    - is the "=?:?:?="	*/
				iEffectLen = ( maxLineLen - line_len - XP_STRLEN( name ) - 7 ) * 3 / 4;
				while( TRUE ){
					int iBufLen;	/* converted buffer's length, not BASE64 */
					if( len > iThreshold )
						len = ResetLen( iThreshold, begin, (int16)wincsid );

					buf1 = (char *) cvtfunc(obj, (unsigned char *)begin, len);
					iBufLen = XP_STRLEN( buf1 );
					XP_ASSERT( iBufLen > 0 );

					/* recal iThreshold each time based on last experience */
					iThreshold = len * iEffectLen / iBufLen;
					if( iBufLen > iEffectLen ){
						/* the converted buffer is too large, we have to
							1. free the buffer;
							2. redo again based on the new iThreshold
						*/
						bChop = TRUE;		/* append CRLFTAB */
						if (buf1 && (buf1 != begin)){
							XP_FREE(buf1);
							buf1 = NULL;
						}
					} else {
						end = begin + len - 1;
						break;
					}
				}
				if (bChop && (NULL!=sep_p)) {
					*sep_p = sep;	/* we are length limited so we do not need this */
					sep = '\0';		/* artifical terminator. So, restore the original character */
					sep_p = NULL;
				}

				if (!buf1)
				{
					XP_FREE(srcbuf);
					XP_FREE(retbuf);
					return NULL;
				}
			}
			else
			{
				buf1 = XP_ALLOC(len + 1);
				if (!buf1)
				{
					XP_FREE(srcbuf);
					XP_FREE(retbuf);
					return NULL;
				}
				XP_MEMCPY(buf1, begin, len);
				*(buf1 + len) = '\0';
			}

			if (wincsid & MULTIBYTE)
			{
				/* converts to Base64 Encoding */
				buf2 = (char *)intlmime_encode_base64_buf(buf1, strlen(buf1));
			}
			else
			{
				/* Converts to Quote Printable Encoding */
				buf2 = (char *)intlmime_encode_qp_buf(buf1);
			}


			if (buf1 && (buf1 != begin))
				XP_FREE(buf1);

			if (buf2 == NULL) /* QUIT if memory allocation failed */
			{
				XP_FREE(srcbuf);
				XP_FREE(retbuf);
				return NULL;
			}

			/* realloc memory for retbuff if necessary, 
			   7: =?...?B?..?=, 3: CR LF TAB */
			convlen = XP_STRLEN(buf2) + XP_STRLEN(name) + 7;
			newsize = convlen + retbuflen + 3 + 2;  /* 2:SEP '\0', 3:CRLFTAB */

			if (newsize > retbufsize)
			{
				char *tempbuf;
				tempbuf = XP_REALLOC(retbuf, newsize);
				if (tempbuf == NULL)  /* QUIT, if not enough memory left */
				{
					XP_FREE(buf2);
					XP_FREE(srcbuf);
					XP_FREE(retbuf);
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

			/* Add encoding tag for base62 and QP */
			XP_STRCAT(buf1, "=?");
			XP_STRCAT(buf1, name );
			if(wincsid & MULTIBYTE)
				XP_STRCAT(buf1, "?B?");
			else
				XP_STRCAT(buf1, "?Q?");
			XP_STRCAT(buf1, buf2);
			XP_STRCAT(buf1, "?=");

			line_len += convlen + 1;  /* 1: SEP */

			XP_FREE(buf2);	/* free base64 buffer */
		}
		else  /* if no 8bit data in the block */
		{
			newsize = retbuflen + len + 2 + 3; /* 2: ',''\0', 3: CRLFTAB */
			if (newsize > retbufsize)
			{
				char *tempbuf;
				tempbuf = XP_REALLOC(retbuf, newsize);
				if (tempbuf == NULL)
				{
					XP_FREE(srcbuf);
					XP_FREE(retbuf);
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

		buf1 = buf1 + XP_STRLEN(buf1);
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
	}
	if (srcbuf)
		XP_FREE(srcbuf);
	return retbuf;
}

/*
	Latin1, latin2:
	   Source --> Quote Printable --> Encoding Info
	Japanese:
	   EUC,JIS,SJIS --> JIS --> Base64 --> Encoding Info
	Others:
	   No conversion  
    flag:   0:   8bit on
	        1:   mime_use_quoted_printable_p
	return:  NULL  if no conversion occured

*/

PRIVATE
char *intl_EncodeMimePartIIStr(char *subject, int16 wincsid, XP_Bool bUseMime, int maxLineLen)
{
	int iSrcLen;
	unsigned char *buf  = NULL;	/* Initial to NULL */
	int16 mail_csid;
   	CCCDataObject	obj = NULL;
	char  *name;
	CCCFunc cvtfunc = NULL;

	if (subject == NULL || *subject == '\0')
		return NULL;

	iSrcLen = XP_STRLEN(subject);
	if (wincsid == 0)
		wincsid = INTL_DefaultWinCharSetID(0) ;

	mail_csid = intlmime_get_outgoing_mime_csid ((int16)wincsid);
	name = (char *)INTL_CsidToCharsetNamePt(mail_csid);
	
	/* check to see if subject are all ascii or not */
	if(intlmime_only_ascii_str(subject))
		return NULL;
		
	if (mail_csid != wincsid)
	{
   		obj = INTL_CreateCharCodeConverter();
		if (obj == NULL)
			return 0;
		/* setup converter from wincsid --> mail_csid */
		INTL_GetCharCodeConverter((int16)wincsid, mail_csid, obj) ;
		cvtfunc = INTL_GetCCCCvtfunc(obj);
	}
	/* Erik said in the case of STATEFUL mail encoding, we should FORCE it to use */
	/* MIME Part2 to get ride of ESC in To: and CC: field, which may introduce more trouble */
	if((bUseMime) || (mail_csid & STATEFUL))/* call intlmime_encode_mail_address */
	{
		buf = (unsigned char *)intlmime_encode_mail_address(wincsid, subject, obj, maxLineLen);
		if(buf == (unsigned char*)subject)	/* no encoding, set return value to NULL */
			buf =  NULL;
	}
	else
	{	/* 8bit, just do conversion if necessary */
		/* In this case, since the conversion routine may reuse the origional buffer */
		/* We better allocate one first- We don't want to reuse the origional buffer */
		
		if ((mail_csid != wincsid) && (cvtfunc))
		{
			char* newbuf = NULL;
			/* Copy buf to newbuf */
			StrAllocCopy(newbuf, subject);
			if(newbuf != NULL)
			{
				buf = (unsigned char *)cvtfunc(obj, (unsigned char*)newbuf, iSrcLen);
				if(buf != (unsigned char*)newbuf)
					XP_FREE(newbuf);
			}
		}
	}
	if (obj)   
		XP_FREE(obj);
	return (char*)buf;
	
	/* IMPORTANT NOTE: */
	/* Return NULL in this interface only mean ther are no conversion */
	/* It does not mean the conversion is store in the origional buffer */
	/* and the length is not change. This is differ from other conversion routine */
}
#endif /* MOZ_MAIL_COMPOSE || MOZ_MAIL_NEWS */

#ifdef MOZ_MAIL_NEWS
#if 0
PUBLIC XP_Bool INTL_FindMimePartIIStr(int16 csid, XP_Bool searchcasesensitive, const char *mimepart2str,const char *s2)
{
	XP_Bool onlyAscii;
	char *ret = NULL;
	char *s1 = (char*)mimepart2str;
	char *conv;
	if((s2 == NULL) || (*s2 == '\0'))	/* if search for NULL string, return TRUE */
		return TRUE;
	if((s1 == NULL) || (*s1 == '\0'))	/* if string is NULL, return FALSE */
		return FALSE;
	
	conv= IntlDecodeMimePartIIStr(mimepart2str, csid, FALSE);
	if(conv)
		s1 = conv;
	onlyAscii = intlmime_only_ascii_str(s1) && intlmime_only_ascii_str(s2);
	if(onlyAscii)	/* for performance reason, let's call the ANSI C routine for ascii only case */
	{
		if(searchcasesensitive)
			ret= strstr( s1, s2);
		else
			ret= strcasestr(s1, s2);
	}
	else
	{
		if(searchcasesensitive)
			ret= INTL_Strstr(csid, s1, s2);
		else
			ret= INTL_Strcasestr(csid, s1, s2);
	}
	if(conv != mimepart2str)
		XP_FREE(conv);
	return (ret != NULL);		/* return TRUE if it find something */
}
#endif
/* 
	NNTP XPAT I18N Support
		INTL_FormatNNTPXPATInNonRFC1522Format and INTL_FormatNNTPXPATInRFC1522Format 
		return the a new string to the caller 
		that could be send to NNTP server for Mail Header Search (use XPAT)
		The caller must free the return string.		
*/

/* This function use the same buffer it pass in, it strip the leading and trialling ISO-2022 ESC */
PRIVATE void intl_strip_leading_and_trial_iso_2022_esc (char* iso_2022_str);
PRIVATE char *intl_xpat_escape ( char *str);

#define ISO_2022_I_CODE(c)	((0x20 <= (c)) && ((c) <= 0x2F))
#define ISO_2022_F_CODE(c)	((0x30 <= (c)) && ((c) <= 0x7E))

PRIVATE void intl_strip_leading_and_trial_iso_2022_esc(char* iso_2022_str)
{
	char* inp = iso_2022_str;
	char* outp = iso_2022_str;
	char* lastescp = NULL;
	
	/* strip leading Escape */
	if(ESC == *inp)	
	{
		for(inp++ ;((*inp) && (ISO_2022_I_CODE(*inp))); /* void */)
			inp++;	/* Skip I Code */
		if(ISO_2022_F_CODE(*inp))	/* Skip F Code */
			inp++;
	}
		
	for( ; (0 != *inp); inp++, outp++)	/* copy data including esc */
	{
		*outp = *inp;
		if(ESC == *outp)	/* remember the last position of esc */
			lastescp = outp;	
	}
	*outp = '\0';	/* NULL terminate */
	
	/* strip trialling Escape if necessary */
	if(lastescp)
	{		
		char* esc_p;	
		for(esc_p = lastescp + 1; ((*esc_p) && (ISO_2022_I_CODE(*esc_p))); /* void */ )
			esc_p++;	/* Skip I Code */
			
		if(ISO_2022_F_CODE(*esc_p))	/* Skip F Code */
			esc_p++;
			
		if('\0' == *esc_p)	/* if it point to our NULL terminate, it is the trialling esp, we take it out */
		{					/* otherwise, it is the esc in the middle, ignore it */
			*lastescp = '\0';
		}
	}
}
#define BETWEEN_A_Z(c)	(('A' <= (c)) && ((c) <= 'Z'))
#define BETWEEN_a_z(c)	(('a' <= (c)) && ((c) <= 'z'))
#define BETWEEN_0_9(c)	(('0' <= (c)) && ((c) <= '9'))
/* 
	Escape Everything except 0-9 A-Z a-z 
   "Common NNTP Extensions" and wildmat(3) does not state clearly what NEED TO BE Escape.
   I look like the * ? [ \ need to be escape , 
   But by trial and error I also find out ^ and $ need to be escape. 
   That's why I just do this ESCAPE MORE THAN WE NEEDED untill we figure out what relly NEED TO BE Escaped    
*/
#define XPAT_NEED_ESCAPE(c)		(! (BETWEEN_A_Z(c) || BETWEEN_a_z(c) || BETWEEN_0_9(c))) 
PRIVATE char *intl_xpat_escape( char *str)
{
	char *result = NULL;
	/* max escaped length is one extra characters for every character in the str. */
	char *scratchBuf = (char*) XP_ALLOC (2*XP_STRLEN(str) + 1);
	if(scratchBuf)
	{
		char *scratchPtr = scratchBuf;
		char ch;
		while ('\0' != (ch = *str++))
		{
			if (XPAT_NEED_ESCAPE(ch))
				*scratchPtr++ = '\\';
			*scratchPtr++ = ch;
		}
		*scratchPtr = '\0';
		result = XP_STRDUP (scratchBuf); /* realloc down to smaller size */
		XP_FREE (scratchBuf);
	}
	return result;
}


/*
	INTL_FormatNNTPXPATInNonRFC1522Format
	1. Convert the data from wincsid to newscsid
	2. Strip Out leading Esc Sequence and trialing Esc Sequence
	3. Always return memory unless memory is not enough. Never have side effect on the pass-in buffer
*/

PUBLIC unsigned char* INTL_FormatNNTPXPATInNonRFC1522Format(int16 wincsid, unsigned char* searchString)
{
	char* temp = NULL;
	char* conv = NULL;
	char* xpat_escape = NULL;
	StrAllocCopy(temp, (char*) searchString);
	XP_ASSERT(temp);	/* Should only come here if Memory Not Enough */
	if(NULL == temp)
		return NULL;
		
	/* Convert text from wincsid to newscsid */
	if(NULL != (conv = (char*)INTL_ConvertLineWithoutAutoDetect(wincsid, INTL_DefaultNewsCharSetID(wincsid), (unsigned char*)temp, XP_STRLEN((char*)temp))))
		XP_FREE(temp);	/* If the conversion do use the same buffer, free the origional one */
	else 
		conv = temp;
	intl_strip_leading_and_trial_iso_2022_esc(conv);
	
	/* Do XPAT escape */
	xpat_escape = intl_xpat_escape(conv);
	if(NULL != conv)
		XP_FREE(conv);
	return (unsigned char*) xpat_escape;
}

#if 0

PUBLIC unsigned char* INTL_FormatNNTPXPATInRFC1522Format(int16 wincsid, unsigned char* searchString)
{
	/* Temp Implementation untill we really support it : Just make a duplication. */
	char* result = NULL;
	StrAllocCopy(result, (char*) searchString);
	XP_ASSERT(result);	/* Should only come here if Memory Not Enough */
	return (unsigned char*) result;
}

#endif

#endif  /* MOZ_MAIL_NEWS */
