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
#include "nsCOMPtr.h"
#include "msgCore.h"
#include "mimerosetta.h"
#include "mimei.h"
#include "libi18n.h"
#include "modmime.h"
#include "prmem.h"
#include "plstr.h"
#include "prefapi.h"
#include "mimebuf.h"
#include "mimemoz2.h"
#include "nsIMimeEmitter.h"
#include "nsCRT.h"
#include "nsIPref.h"
#include "nsEscape.h"
#include "nsMimeTransition.h"
#include "nsMsgMessageFlags.h"
#include "nsMimeAddress.h"
#include "comi18n.h"
#include "nsMailHeaders.h"
#include "msgCore.h"
#include "nsMimeURLUtils.h"
#include "nsMimeStringResources.h"
#include "mimemoz2.h"

// Forward declares...
extern "C" char     *MIME_DecodeMimePartIIStr(const char *header, char *charset);
extern "C"  char    *MIME_StripContinuations(char *original);
int MimeHeaders_build_heads_list(MimeHeaders *hdrs);

static int
MimeHeaders_convert_rfc1522(MimeDisplayOptions *opt,
							              const char *input, PRInt32 input_length,
							              char **output_ret, PRInt32 *output_length_ret);

char *
MimeHeaders_convert_header_value(MimeDisplayOptions *opt, char **value)
{
  char        *converted;
  PRInt32     converted_length;

  if ((!value) || (!*value))
    return *value;

  PRInt32 contents_length = PL_strlen(*value);
  int     status = MimeHeaders_convert_rfc1522(opt, *value, contents_length,
					                          				   &converted, &converted_length);
  if (status == 0) 
  {
    PR_FREEIF(*value);
    *value = converted;
  }

  return(*value);
}


MimeHeaders *
MimeHeaders_new (void)
{
  MimeHeaders *hdrs = (MimeHeaders *) PR_MALLOC(sizeof(MimeHeaders));
  if (!hdrs) return 0;

  memset(hdrs, 0, sizeof(*hdrs));
  hdrs->done_p = PR_FALSE;

  return hdrs;
}

void
MimeHeaders_free (MimeHeaders *hdrs)
{
  if (!hdrs) return;
  PR_FREEIF(hdrs->all_headers);
  PR_FREEIF(hdrs->heads);
  PR_FREEIF(hdrs->obuffer);
  PR_FREEIF(hdrs->munged_subject);
  hdrs->obuffer_fp = 0;
  hdrs->obuffer_size = 0;

# ifdef DEBUG__
  {
	int i, size = sizeof(*hdrs);
	PRUint32 *array = (PRUint32*) hdrs;
	for (i = 0; i < (size / sizeof(*array)); i++)
	  array[i] = (PRUint32) 0xDEADBEEF;
  }
# endif /* DEBUG */

  PR_Free(hdrs);
}

int
MimeHeaders_parse_line (const char *buffer, PRInt32 size, MimeHeaders *hdrs)
{
  int status = 0;
  int desired_size;

  PR_ASSERT(hdrs);
  if (!hdrs) return -1;

  /* Don't try and feed me more data after having fed me a blank line... */
  PR_ASSERT(!hdrs->done_p);
  if (hdrs->done_p) return -1;

  if (!buffer || size == 0 || *buffer == CR || *buffer == LF)
	{
	  /* If this is a blank line, we're done.
	   */
	  hdrs->done_p = PR_TRUE;
	  return MimeHeaders_build_heads_list(hdrs);
	}

  /* Tack this data on to the end of our copy.
   */
  desired_size = hdrs->all_headers_fp + size + 1;
  if (desired_size >= hdrs->all_headers_size)
	{
	  status = mime_GrowBuffer (desired_size, sizeof(char), 255,
							   &hdrs->all_headers, &hdrs->all_headers_size);
	  if (status < 0) return status;
	}
  nsCRT::memcpy(hdrs->all_headers+hdrs->all_headers_fp, buffer, size);
  hdrs->all_headers_fp += size;

  return 0;
}

MimeHeaders *
MimeHeaders_copy (MimeHeaders *hdrs)
{
  MimeHeaders *hdrs2;
  if (!hdrs) return 0;

  hdrs2 = (MimeHeaders *) PR_MALLOC(sizeof(*hdrs));
  if (!hdrs2) return 0;
  memset(hdrs2, 0, sizeof(*hdrs2));

  if (hdrs->all_headers)
	{
	  hdrs2->all_headers = (char *) PR_MALLOC(hdrs->all_headers_fp);
	  if (!hdrs2->all_headers)
		{
		  PR_Free(hdrs2);
		  return 0;
		}
	  nsCRT::memcpy(hdrs2->all_headers, hdrs->all_headers, hdrs->all_headers_fp);

	  hdrs2->all_headers_fp   = hdrs->all_headers_fp;
	  hdrs2->all_headers_size = hdrs->all_headers_fp;
	}

  hdrs2->done_p = hdrs->done_p;

  if (hdrs->heads)
	{
	  int i;
	  hdrs2->heads = (char **) PR_MALLOC(hdrs->heads_size
										* sizeof(*hdrs->heads));
	  if (!hdrs2->heads)
		{
		  PR_FREEIF(hdrs2->all_headers);
		  PR_Free(hdrs2);
		  return 0;
		}
	  hdrs2->heads_size = hdrs->heads_size;
	  for (i = 0; i < hdrs->heads_size; i++)
		{
		  hdrs2->heads[i] = (hdrs2->all_headers +
							 (hdrs->heads[i] - hdrs->all_headers));
		}
	}
  return hdrs2;
}

int
MimeHeaders_build_heads_list(MimeHeaders *hdrs)
{
  char *s;
  char *end;
  int i;
  PR_ASSERT(hdrs);
  if (!hdrs) return -1;

  PR_ASSERT(hdrs->done_p && !hdrs->heads);
  if (!hdrs->done_p || hdrs->heads)
	return -1;

  if (hdrs->all_headers_fp == 0)
	{
	  /* Must not have been any headers (we got the blank line right away.) */
	  PR_FREEIF (hdrs->all_headers);
	  hdrs->all_headers_size = 0;
	  return 0;
	}

  /* At this point, we might as well realloc all_headers back down to the
	 minimum size it must be (it could be up to 1k bigger.)  But don't
	 bother if we're only off by a tiny bit. */
  PR_ASSERT(hdrs->all_headers_fp <= hdrs->all_headers_size);
  if (hdrs->all_headers_fp + 60 <= hdrs->all_headers_size)
	{
	  char *ls = (char *)PR_Realloc(hdrs->all_headers, hdrs->all_headers_fp);
	  if (ls) /* can this ever fail?  we're making it smaller... */
		{
		  hdrs->all_headers = ls;  /* in case it got relocated */
		  hdrs->all_headers_size = hdrs->all_headers_fp;
		}
	}


  /* First go through and count up the number of headers in the block.
   */
  end = hdrs->all_headers + hdrs->all_headers_fp;
  for (s = hdrs->all_headers; s <= end-1; s++)
	{
	  if (s <= (end-1) && s[0] == CR && s[1] == LF) /* CRLF -> LF */
		s++;

	  if ((s[0] == CR || s[0] == LF) &&			/* we're at a newline, and */
		  (s >= (end-1) ||						/* we're at EOF, or */
		   !(s[1] == ' ' || s[1] == '\t')))		/* next char is nonwhite */
		hdrs->heads_size++;
	}
	  
  /* Now allocate storage for the pointers to each of those headers.
   */
  hdrs->heads = (char **) PR_MALLOC((hdrs->heads_size + 1) * sizeof(char *));
  if (!hdrs->heads)
	return MIME_OUT_OF_MEMORY;
  memset(hdrs->heads, 0, (hdrs->heads_size + 1) * sizeof(char *));


  /* Now make another pass through the headers, and this time, record the
	 starting position of each header.
   */

  i = 0;
  hdrs->heads[i++] = hdrs->all_headers;
  s = hdrs->all_headers;

  while (s <= end)
	{
	SEARCH_NEWLINE:
	  while (s <= end-1 && *s != CR && *s != LF)
		s++;

	  if (s+1 >= end)
		break;

	  /* If "\r\n " or "\r\n\t" is next, that doesn't terminate the header. */
	  else if (s+2 < end &&
			   (s[0] == CR  && s[1] == LF) &&
			   (s[2] == ' ' || s[2] == '\t'))
		{
		  s += 3;
		  goto SEARCH_NEWLINE;
		}
	  /* If "\r " or "\r\t" or "\n " or "\n\t" is next, that doesn't terminate
		 the header either. */
	  else if ((s[0] == CR  || s[0] == LF) &&
			   (s[1] == ' ' || s[1] == '\t'))
		{
		  s += 2;
		  goto SEARCH_NEWLINE;
		}

	  /* At this point, `s' points before a header-terminating newline.
		 Move past that newline, and store that new position in `heads'.
	   */
	  if (*s == CR) s++;
	  if (*s == LF) s++;

	  if (s < end)
		{
		  PR_ASSERT(! (i > hdrs->heads_size));
		  if (i > hdrs->heads_size) return -1;
		  hdrs->heads[i++] = s;
		}
	}

  return 0;
}

char *
MimeHeaders_get (MimeHeaders *hdrs, const char *header_name,
				 PRBool strip_p, PRBool all_p)
{
  int i;
  int name_length;
  char *result = 0;

/*  PR_ASSERT(hdrs); cause delete message problem in WinFE */
  if (!hdrs) return 0;
  PR_ASSERT(header_name);
  if (!header_name) return 0;

  /* Specifying strip_p and all_p at the same time doesn't make sense... */
  PR_ASSERT(!(strip_p && all_p));

  /* One shouldn't be trying to read headers when one hasn't finished
	 parsing them yet... but this can happen if the message ended
	 prematurely, and has no body at all (as opposed to a null body,
	 which is more normal.)   So, if we try to read from the headers,
	 let's assume that the headers are now finished.  If they aren't
	 in fact finished, then a later attempt to write to them will assert.
   */
  if (!hdrs->done_p)
	{
	  int status;
	  hdrs->done_p = PR_TRUE;
	  status = MimeHeaders_build_heads_list(hdrs);
	  if (status < 0) return 0;
	}

  if (!hdrs->heads)	  /* Must not have been any headers. */
	{
	  PR_ASSERT(hdrs->all_headers_fp == 0);
	  return 0;
	}

  name_length = PL_strlen(header_name);

  for (i = 0; i < hdrs->heads_size; i++)
	{
	  char *head = hdrs->heads[i];
	  char *end = (i == hdrs->heads_size-1
				   ? hdrs->all_headers + hdrs->all_headers_fp
				   : hdrs->heads[i+1]);
	  char *colon, *ocolon;

	  PR_ASSERT(head);
	  if (!head) continue;

	  /* Quick hack to skip over BSD Mailbox delimiter. */
	  if (i == 0 && head[0] == 'F' && !PL_strncmp(head, "From ", 5))
		continue;

	  /* Find the colon. */
	  for (colon = head; colon < end; colon++)
		if (*colon == ':') break;

	  if (colon >= end) continue;

	  /* Back up over whitespace before the colon. */
	  ocolon = colon;
	  for (; colon > head && IS_SPACE(colon[-1]); colon--)
		;

	  /* If the strings aren't the same length, it doesn't match. */
	  if (name_length != colon - head )
		continue;

	  /* If the strings differ, it doesn't match. */
	  if (PL_strncasecmp(header_name, head, name_length))
		continue;

	  /* Otherwise, we've got a match. */
	  {
		char *contents = ocolon + 1;
		char *s;

		/* Skip over whitespace after colon. */
		while (contents <= end && IS_SPACE(*contents))
		  contents++;

		/* If we're supposed to strip at the frist token, pull `end' back to
		   the first whitespace or ';' after the first token.
		 */
		if (strip_p)
		  {
			for (s = contents;
				 s <= end && *s != ';' && *s != ',' && !IS_SPACE(*s);
				 s++)
			  ;
			end = s;
		  }

		/* Now allocate some storage.
		   If `result' already has a value, enlarge it.
		   Otherwise, just allocate a block.
		   `s' gets set to the place where the new data goes.
		 */
		if (!result)
		  {
			result = (char *) PR_MALLOC(end - contents + 1);
			if (!result)
			  return 0;
			s = result;
		  }
		else
		  {
			PRInt32 L = PL_strlen(result);
			s = (char *) PR_Realloc(result, (L + (end - contents + 10)));
			if (!s)
			  {
				PR_Free(result);
				return 0;
			  }
			result = s;
			s = result + L;

			/* Since we are tacking more data onto the end of the header
			   field, we must make it be a well-formed continuation line,
			   by seperating the old and new data with CR-LF-TAB.
			 */
			*s++ = ',';				/* #### only do this for addr headers? */
			*s++ = MSG_LINEBREAK[0];
# if (MSG_LINEBREAK_LEN == 2)
			*s++ = MSG_LINEBREAK[1];
# endif
			*s++ = '\t';
		  }

		/* Take off trailing whitespace... */
		while (end > contents && IS_SPACE(end[-1]))
		  end--;

		if (end > contents)
		  {
		    /* Now copy the header's contents in...
		     */
		    nsCRT::memcpy(s, contents, end - contents);
		    s[end - contents] = 0;
		  }
		else
		  {
		    s[0] = 0;
		  }

		/* If we only wanted the first occurence of this header, we're done. */
		if (!all_p) break;
	  }
	}

  if (result && !*result)  /* empty string */
	{
	  PR_Free(result);
	  return 0;
	}

  return result;
}

char *
MimeHeaders_get_parameter (const char *header_value, const char *parm_name, 
						   char **charset, char **language)
{
  const char *str;
  char *s = NULL; /* parm value to be returned */
  PRInt32 parm_len;
  if (!header_value || !parm_name || !*header_value || !*parm_name)
	return 0;

  /* The format of these header lines is
	 <token> [ ';' <token> '=' <token-or-quoted-string> ]*
   */

  if (charset) *charset = NULL;
  if (language) *language = NULL;

  str = header_value;
  parm_len = PL_strlen(parm_name);

  /* Skip forward to first ';' */
  for (; *str && *str != ';' && *str != ','; str++)
	;
  if (*str)
	str++;
  /* Skip over following whitespace */
  for (; *str && IS_SPACE(*str); str++)
	;
  if (!*str)
	return 0;

  while (*str)
	{
	  const char *token_start = str;
	  const char *token_end = 0;
	  const char *value_start = str;
	  const char *value_end = 0;

	  PR_ASSERT(!IS_SPACE(*str)); /* should be after whitespace already */

	  /* Skip forward to the end of this token. */
	  for (; *str && !IS_SPACE(*str) && *str != '=' && *str != ';'; str++)
		;
	  token_end = str;

	  /* Skip over whitespace, '=', and whitespace */
	  while (IS_SPACE (*str)) str++;
	  if (*str == '=') str++;
	  while (IS_SPACE (*str)) str++;

	  if (*str != '"')
		{
		  /* The value is a token, not a quoted string. */
		  value_start = str;
		  for (value_end = str;
			   *value_end && !IS_SPACE (*value_end) && *value_end != ';';
			   value_end++)
			;
		  str = value_end;
		}
	  else
		{
		  /* The value is a quoted string. */
		  str++;
		  value_start = str;
		  for (value_end = str; *value_end; value_end++)
			{
			  if (*value_end == '\\')
				value_end++;
			  else if (*value_end == '"')
				break;
			}
		  str = value_end+1;
		}

	  /* See if this is the parameter we're looking for.
		 If so, copy it and return.
	   */
	  if (token_end - token_start == parm_len &&
		  !PL_strncasecmp(token_start, parm_name, parm_len))
		{
		  s = (char *) PR_MALLOC ((value_end - value_start) + 1);
		  if (! s) return 0;  /* MIME_OUT_OF_MEMORY */
		  nsCRT::memcpy (s, value_start, value_end - value_start);
		  s [value_end - value_start] = 0;
		  /* if the parameter spans across multiple lines we have to strip out the
			 line continuatio -- jht 4/29/98 */
		  MIME_StripContinuations(s);
		  return s;
		}
	  else if (token_end - token_start > parm_len &&
			   !PL_strncasecmp(token_start, parm_name, parm_len) &&
			   *(token_start+parm_len) == '*')
	  {
		  /* RFC2231 - The legitimate parm format can be:
			 title*=us-ascii'en-us'This%20is%20weired.
			    or
			 title*0*=us-ascii'en'This%20is%20weired.%20We
			 title*1*=have%20to%20support%20this.
			 title*3="Else..."
			    or
			 title*0="Hey, what you think you are doing?"
			 title*1="There is no charset and language info."
		   */
		  const char *cp = token_start+parm_len+1; /* 1st char pass '*' */
		  PRBool needUnescape = *(token_end-1) == '*';
		  if ((*cp == '0' && needUnescape) || (token_end-token_start == parm_len+1))
		  {
			  const char *s_quote1 = PL_strchr(value_start, 0x27);
			  const char *s_quote2 = (char *) (s_quote1 ? PL_strchr(s_quote1+1, 0x27) : NULL);
			  PR_ASSERT(s_quote1 && s_quote2);
			  if (charset && s_quote1 > value_start && s_quote1 < value_end)
			  {
				  *charset = (char *) PR_MALLOC(s_quote1-value_start+1);
				  if (*charset)
				  {
					  nsCRT::memcpy(*charset, value_start, s_quote1-value_start);
					  *(*charset+(s_quote1-value_start)) = 0;
				  }
			  }
			  if (language && s_quote1 && s_quote2 && s_quote2 > s_quote1+1 &&
				  s_quote2 < value_end)
			  {
				  *language = (char *) PR_MALLOC(s_quote2-(s_quote1+1)+1);
				  if (*language)
				  {
					  nsCRT::memcpy(*language, s_quote1+1, s_quote2-(s_quote1+1));
					  *(*language+(s_quote2-(s_quote1+1))) = 0;
				  }
			  }
			  if (s_quote2 && s_quote2+1 < value_end)
			  {
				  PR_ASSERT(!s);
				  s = (char *) PR_MALLOC(value_end-(s_quote2+1)+1);
				  if (s)
				  {
					  nsCRT::memcpy(s, s_quote2+1, value_end-(s_quote2+1));
					  *(s+(value_end-(s_quote2+1))) = 0;
					  if (needUnescape)
					  {
						  nsUnescape(s);
						  if (token_end-token_start == parm_len+1)
							  return s; /* we done; this is the simple case of
										   encoding charset and language info
										 */
					  }
				  }
			  }
		  }
		  else if (XP_IS_DIGIT(*cp))
		  {
			  PRInt32 len = 0;
			  char *ns = NULL;
			  if (s)
			  {
				  len = PL_strlen(s);
				  ns = (char *) PR_Realloc(s, len+(value_end-value_start)+1);
				  if (!ns)
                    {
					  PR_FREEIF(s);
                    }
				  else if (ns != s)
					  s = ns;
			  }
			  else if (*cp == '0') /* must be; otherwise something is wrong */
			  {
				  s = (char *) PR_MALLOC(value_end-value_start+1);
			  }
			  /* else {} something is really wrong; out of memory */
			  if (s)
			  {
				  nsCRT::memcpy(s+len, value_start, value_end-value_start);
				  *(s+len+(value_end-value_start)) = 0;
				  if (needUnescape)
					  nsUnescape(s+len);
			  }
		  }
	  }

	  /* str now points after the end of the value.
		 skip over whitespace, ';', whitespace. */
	  while (IS_SPACE (*str)) str++;
	  if (*str == ';') str++;
	  while (IS_SPACE (*str)) str++;
	}
  return s;
}


#define MimeHeaders_write(OPT,DATA,LENGTH) \
  	MimeOptions_write((OPT), (DATA), (LENGTH), PR_TRUE);

#if 0
static char *
MimeHeaders_default_news_link_generator (const char *dest, void *closure,
	                    									 MimeHeaders *headers)
{
  /* This works as both the generate_news_url_fn and as the
	 generate_reference_url_fn. */
  char *prefix = "news:";
  char *new_dest = nsEscape(dest, url_XAlphas);
  char *result = (char *) PR_MALLOC (PL_strlen (new_dest) +
									PL_strlen (prefix) + 1);
  if (result)
	{
	  PL_strcpy (result, prefix);
	  PL_strcat (result, new_dest);
	}
  nsCRT::free (new_dest);
  return result;
}

static char *mime_escape_quotes (char *src)
{
	/* Make sure quotes are escaped with a backslash */
	char *dst = (char *)PR_MALLOC((2 * PL_strlen(src)) + 1);
	if (dst)
	{
		char *walkDst = dst;
		while (*src)
		{
			/* here's a big hack. double quotes, even if escaped, produce JS errors, 
			 * so we'll just whack a double quote into a single quote. This string
			 * is just eye candy anyway.
			 */
			if (*src == '\"') 
				*src = '\'';

			if (*src == '\'') /* is it a quote? */
				if (walkDst == dst || *(src-1) != '\\')  /* is it escaped? */
					*walkDst++ = '\\';

			*walkDst++ = *src++;
		}
		*walkDst = '\0';
	}
	return dst;
}

static char *
MimeHeaders_default_addbook_link_generator (const char *dest, void *closure,
										   MimeHeaders *headers)
{
  char* names;
  char* addresses;
  char* name = NULL;
  char* addr = NULL;
  char* unquotedName;
  char* unquotedAddr;
  char* converted;
  char* charset;
  PRInt16 winCharSetID;
  char* result = NULL;
  char* tmp;
  char* tmp2;
  char* mouseOverText = NULL;
  int j;
  int num = ParseRFC822Addresses(dest, &names, &addresses);
  char charsetName[128];
  charsetName[0] = 0;
  PR_ASSERT(num >= 1);
  for (j=0 ; j<num ; j++) {
	if (addr) {
	  addr = addr + PL_strlen(addr) + 1;
	  name = name + PL_strlen(name) + 1;
	} else {
	  addr = addresses;
	  name = names;
	}
	if (!addr || !*addr) continue;

	unquotedName = NULL;
	unquotedAddr = NULL;
	UnquotePhraseOrAddr (addr, &unquotedAddr);
	if (!unquotedAddr) 
		continue;
	if (name)
		UnquotePhraseOrAddr (name, &unquotedName);

	winCharSetID = INTL_DefaultWinCharSetID(0);
	converted = MIME_DecodeMimePartIIStr((const char *) unquotedName, charsetName);
	if (converted && (converted != unquotedName)) {
		PR_FREEIF(unquotedName);
		unquotedName = converted;
//TODO: naoki - After the decode, we may need to do a charset conversion.
// If this is to handed to a widget then probably convert to unicode.
// If this is put inside an HTML then convet to the content type charset of the HTML.
    //		INTL_CharSetIDToName(winCharSetID, charsetName);
		if (charsetName[0]) {
			charset = PR_smprintf(";cs=%s", charsetName);
		} else {
			charset = PR_smprintf("");
		}
	} else {
		charset = PR_smprintf("");
	}
	if (unquotedName && *unquotedName)
		tmp = PR_smprintf("begin:vcard\nfn%s:%s\nemail;internet:%s\nend:vcard\n",
		                  charset, unquotedName, unquotedAddr);
	else
		tmp = PR_smprintf("begin:vcard\nemail;internet:%s\nend:vcard\n", unquotedAddr);

	/* Since the addbook: URL is a little gross to look at, try to use Javascript's 
	 * mouseOver event to plug some more human readable text into the status bar.
	 * If the user doesn't have JS enabled, they just get the gross URL.
	 */
	if (closure && !mouseOverText)
	{
		/* Make sure we don't feed any unescaped single or double quotes to the
		 * Javascript interpreter, since that will cause the string termination
		 * to be wrong, and things go downhill fast from there
		 */
		char *jsSafeName = mime_escape_quotes ((unquotedName && *unquotedName) ? unquotedName : unquotedAddr);
		if (jsSafeName)
		{
      char  *mouseText = MimeGetStringByID(MIME_MSG_ADDBOOK_MOUSEOVER_TEXT);
			char *localizedPart = PR_smprintf (mouseText, jsSafeName);
      PR_FREEIF(mouseText);
			if (localizedPart)
			{
				mouseOverText = PR_smprintf ("onMouseOver=\"window.status='%s'; return true\" onMouseOut=\"window.status=''\"", localizedPart);
				PR_Free(localizedPart);
				*((char**)closure) = mouseOverText;
			}
			PR_Free(jsSafeName);
		}
	}

	PR_FREEIF(unquotedAddr);
	PR_FREEIF(unquotedName);
	PR_FREEIF(charset);

	if (!tmp) break;
	tmp2 = nsEscape(tmp, url_XAlphas);
	PR_Free(tmp);
	if (!tmp2) break;
	result = PR_smprintf("addbook:add?vcard=%s", tmp2);
	nsCRT::free(tmp2);
	break;
  }
  PR_FREEIF(names);
  PR_FREEIF(addresses);
  return result;
}
#endif

#define MimeHeaders_grow_obuffer(hdrs, desired_size) \
  ((((long) (desired_size)) >= ((long) (hdrs)->obuffer_size)) ? \
   mime_GrowBuffer ((desired_size), sizeof(char), 255, \
				   &(hdrs)->obuffer, &(hdrs)->obuffer_size) \
   : 0)


static int
MimeHeaders_convert_rfc1522(MimeDisplayOptions *opt,
							const char *input, PRInt32 input_length,
							char **output_ret, PRInt32 *output_length_ret)
{
  *output_ret = 0;
  *output_length_ret = 0;
  if (input && *input && opt && opt->rfc1522_conversion_fn)
	{
	  char *converted = 0;
	  PRInt32 converted_len = 0;
    const char *output_charset = "UTF-8";
	  int status =
		opt->rfc1522_conversion_fn(input, input_length,
								   0, output_charset,  /* no input charset? */
								   &converted, &converted_len,
								   opt->stream_closure);
	  if (status < 0)
		{
		  PR_FREEIF(converted);
		  return status;
		}

	  if (converted)
		{
		  *output_ret = converted;
		  *output_length_ret = converted_len;
		}
	}
  return 0;
}

#if 0
static char *
MimeHeaders_localize_header_name(char *name, MimeDisplayOptions *opt)
{
  if (!PL_strcasecmp(name,HEADER_BCC))
    return MimeGetStringByID(MIME_MHTML_BCC);
  if (!PL_strcasecmp(name,HEADER_CC))
    return MimeGetStringByID(MIME_MHTML_CC);
  if (!PL_strcasecmp(name,HEADER_DATE))
    return MimeGetStringByID(MIME_MHTML_DATE);
  if (!PL_strcasecmp(name,HEADER_FOLLOWUP_TO))
    return MimeGetStringByID(MIME_MHTML_FOLLOWUP_TO);
  if (!PL_strcasecmp(name,HEADER_FROM))
    return MimeGetStringByID(MIME_MHTML_FROM);
  if (!PL_strcasecmp(name,HEADER_MESSAGE_ID))
    return MimeGetStringByID(MIME_MHTML_MESSAGE_ID);
  if (!PL_strcasecmp(name,HEADER_NEWSGROUPS))
    return MimeGetStringByID(MIME_MHTML_NEWSGROUPS);
  if (!PL_strcasecmp(name,HEADER_ORGANIZATION))
    return MimeGetStringByID(MIME_MHTML_ORGANIZATION);
  if (!PL_strcasecmp(name,HEADER_REFERENCES))
    return MimeGetStringByID(MIME_MHTML_REFERENCES);
  if (!PL_strcasecmp(name,HEADER_REPLY_TO))
    return MimeGetStringByID(MIME_MHTML_REPLY_TO);
  if (!PL_strcasecmp(name,HEADER_RESENT_CC))
    return MimeGetStringByID(MIME_MHTML_RESENT_CC);
  if (!PL_strcasecmp(name,HEADER_RESENT_COMMENTS))
    return MimeGetStringByID(MIME_MHTML_RESENT_COMMENTS);
  if (!PL_strcasecmp(name,HEADER_RESENT_DATE))
    return MimeGetStringByID(MIME_MHTML_RESENT_DATE);
  if (!PL_strcasecmp(name,HEADER_RESENT_FROM))
    return MimeGetStringByID(MIME_MHTML_RESENT_FROM);
  if (!PL_strcasecmp(name,HEADER_RESENT_MESSAGE_ID))
    return MimeGetStringByID(MIME_MHTML_RESENT_MESSAGE_ID);
  if (!PL_strcasecmp(name,HEADER_RESENT_SENDER))
    return MimeGetStringByID(MIME_MHTML_RESENT_SENDER);
  if (!PL_strcasecmp(name,HEADER_RESENT_TO))
    return MimeGetStringByID(MIME_MHTML_RESENT_TO);
  if (!PL_strcasecmp(name,HEADER_SENDER))
    return MimeGetStringByID(MIME_MHTML_SENDER);
  if (!PL_strcasecmp(name,HEADER_SUBJECT))
    return MimeGetStringByID(MIME_MHTML_SUBJECT);
  if (!PL_strcasecmp(name,HEADER_TO))
    return MimeGetStringByID(MIME_MHTML_TO);
  if (!PL_strcasecmp(name,HEADER_SUBJECT))
    return MimeGetStringByID(MIME_MHTML_SUBJECT);

  return NULL;  
}
#endif

int
MimeHeaders_write_all_headers (MimeHeaders *hdrs, MimeDisplayOptions *opt, PRBool attachment)
{
  int status = 0;
  int i;
  PRBool wrote_any_p = PR_FALSE;

  PR_ASSERT(hdrs);
  if (!hdrs) 
    return -1;

  /* One shouldn't be trying to read headers when one hasn't finished
	 parsing them yet... but this can happen if the message ended
	 prematurely, and has no body at all (as opposed to a null body,
	 which is more normal.)   So, if we try to read from the headers,
	 let's assume that the headers are now finished.  If they aren't
	 in fact finished, then a later attempt to write to them will assert.
   */
  if (!hdrs->done_p)
	{
	  hdrs->done_p = PR_TRUE;
	  status = MimeHeaders_build_heads_list(hdrs);
	  if (status < 0) return 0;
	}

  for (i = 0; i < hdrs->heads_size; i++)
  {
    char *head = hdrs->heads[i];
    char *end = (i == hdrs->heads_size-1
                      ? hdrs->all_headers + hdrs->all_headers_fp
                      : hdrs->heads[i+1]);
    char *colon, *ocolon;
    char *contents;
    char *name = 0;
    char *c2 = 0;
    
    /* Hack for BSD Mailbox delimiter. */
    if (i == 0 && head[0] == 'F' && !PL_strncmp(head, "From ", 5))
    {
      /* For now, we don't really want this header to be output so
         we are going to just continue */
      continue;
      /* colon = head + 4; contents = colon + 1; */
    }
    else
    {
      /* Find the colon. */
      for (colon = head; colon < end; colon++)
        if (*colon == ':') break;
        
        if (colon >= end) continue;   /* junk */
        
        /* Back up over whitespace before the colon. */
        ocolon = colon;
        for (; colon > head && IS_SPACE(colon[-1]); colon--)
          ;
        
        contents = ocolon + 1;
    }
    
    /* Skip over whitespace after colon. */
    while (contents <= end && IS_SPACE(*contents))
      contents++;
    
    /* Take off trailing whitespace... */
    while (end > contents && IS_SPACE(end[-1]))
      end--;
    
    name = (char *)PR_MALLOC(colon - head + 1);
    if (!name) return MIME_OUT_OF_MEMORY;
    nsCRT::memcpy(name, head, colon - head);
    name[colon - head] = 0;
    
    c2 = (char *)PR_MALLOC(end - contents + 1);
    if (!c2)
    {
      PR_Free(name);
      return MIME_OUT_OF_MEMORY;
    }
    nsCRT::memcpy(c2, contents, end - contents);
    c2[end - contents] = 0;
    
    if (attachment)
      status = mimeEmitterAddAttachmentField(opt, name, 
                                MimeHeaders_convert_header_value(opt, &c2));
    else
      status = mimeEmitterAddHeaderField(opt, name, 
                                MimeHeaders_convert_header_value(opt, &c2));

    PR_Free(name);
    PR_Free(c2);
    
    if (status < 0) return status;
    if (!wrote_any_p) 
      wrote_any_p = (status > 0);
  }

  return 1;
}

/* 
 * This routine now drives the emitter by printing all of the 
 * attachment headers.
 */
int
MimeHeaders_write_attachment_box(MimeHeaders *hdrs,
								 MimeDisplayOptions *opt,
								 const char *content_type,
								 const char *encoding,
								 const char *lname,
								 const char *lname_url,
								 const char *body)
{
  int     status = 0;

  // RICHIE SHERRY 
  // Writing a header "box" is not needed anymore since we are doing
  // attachments a whole new way... I need to clean this up and this
  // routine will probably go away for good...but until then, just return
  // success
  //
  nsIPref *pref = GetPrefServiceManager(opt);   // Pref service manager
  PRBool      mimeXULOutput = PR_FALSE;
  
  if (pref)
    pref->GetBoolPref("mail.mime_xul_output", &mimeXULOutput);

  if (!mimeXULOutput)
  {
    mimeEmitterStartAttachment(opt, lname, content_type, lname_url);
    status = MimeHeaders_write_all_headers (hdrs, opt, PR_TRUE);
    mimeEmitterAddAttachmentField(opt, HEADER_X_MOZILLA_PART_URL, lname_url);
    mimeEmitterEndAttachment(opt);
  }

  if (status < 0) 
    return status;
  else
    return 0;
}

#ifdef MOZ_SECURITY
HG99401
#endif /* MOZ_SECURITY */

/* Strip CR+LF+<whitespace> runs within (original).
   Since the string at (original) can only shrink,
   this conversion is done in place. (original)
   is returned. */
extern "C" char *
MIME_StripContinuations(char *original)
{
	char *p1, *p2;

	/* If we were given a null string, return it as is */
	if (!original) return NULL;

	/* Start source and dest pointers at the beginning */
	p1 = p2 = original;

	while(*p2)
	{
		/* p2 runs ahead at (CR and/or LF) + <space> */
		if ((p2[0] == CR) || (p2[0] == LF))
		{
			/* move past (CR and/or LF) + whitespace following */	
			do
			{
				p2++;
			}
			while((*p2 == CR) || (*p2 == LF) || IS_SPACE(*p2));

			if (*p2 == '\0') continue; /* drop out of loop at end of string*/
		}

		/* Copy the next non-linebreaking char */
		*p1 = *p2;
		p1++; p2++;
	}
	*p1 = '\0';

	return original;
}

extern PRInt16 INTL_DefaultMailToWinCharSetID(PRInt16 csid);

/* Given text purporting to be a qtext header value, strip backslashes that
	may be escaping other chars in the string. */
char *
mime_decode_filename(char *name)
{
	char *s = name, *d = name;
	char *cvt, *returnVal = NULL;
	PRInt16 mail_csid = CS_DEFAULT, win_csid = CS_DEFAULT;
  char charsetName[128];
  charsetName[0] = 0;
  
	while (*s)
	{
		/* Remove backslashes when they are used to escape special characters. */
		if ((*s == '\\') &&
			((*(s+1) == CR) || (*(s+1) == LF) || (*(s+1) == '"') || (*(s+1) == '\\')))
			s++; /* take whatever char follows the backslash */
		if (*s)
			*d++ = *s++;
	}
	*d = 0;
	returnVal = name;
	
	/* If there is a MIME-2 encoded-word in the string, 
		get the charset of the first one and decode to that charset. */
	s = PL_strstr(returnVal, "=?");
	if (s)
	{
		s += 2;
		d = PL_strchr(s, '?');
		if (d) *d = '\0';
		mail_csid = INTL_CharSetNameToID(s);
		if (d) *d = '?';
		win_csid = INTL_DocToWinCharSetID(mail_csid);

    cvt = MIME_DecodeMimePartIIStr(returnVal, charsetName);

    // rhp - trying to fix header conversion bug
    //
		if (cvt && cvt != returnVal)
    {
      char *newString = nsnull;
      PRInt32 res = MIME_ConvertString(charsetName, "UTF-8", cvt, &newString); 
      if ( (res != 0) || (!newString) )
        returnVal = cvt;
      else
      {
        PR_FREEIF(cvt);
        returnVal = newString;
      }
    }
	}

//TODO: naoki - We now have a charset name returned and no need for the hack.
// Check if charset is JIS then apply the conversion.
	/* Seriously ugly hack. If the first three characters of the filename 
	   are <ESC>$B, then we know the filename is in JIS and should be 
	   converted to either SJIS or EUC-JP. */ 
	if ((PL_strlen(returnVal) > 3) && 
		(returnVal[0] == 0x1B) && (returnVal[1] == '$') && (returnVal[2] == 'B')) 
	  { 
		PRInt16 dest_csid = INTL_DocToWinCharSetID(CS_JIS); 
		
		cvt = (char *) INTL_ConvertLineWithoutAutoDetect(CS_JIS, dest_csid, (unsigned char *)returnVal, PL_strlen(returnVal)); 
		if (cvt && cvt != returnVal) 
		  { 
			if (returnVal != name) PR_Free(returnVal); 
			returnVal = cvt; 
		  } 
	  } 
	
	return returnVal;
}

/* Pull the name out of some header or another.  Order is:
   Content-Disposition: XXX; filename=NAME (RFC 1521/1806)
   Content-Type: XXX; name=NAME (RFC 1341)
   Content-Name: NAME (no RFC, but seen to occur)
   X-Sun-Data-Name: NAME (no RFC, but used by MailTool)
 */
char *
MimeHeaders_get_name(MimeHeaders *hdrs)
{
  char *s = 0, *name = 0, *cvt = 0;

  s = MimeHeaders_get(hdrs, HEADER_CONTENT_DISPOSITION, PR_FALSE, PR_FALSE);
  if (s)
	{
	  name = MimeHeaders_get_parameter(s, HEADER_PARM_FILENAME, NULL, NULL);
	  PR_Free(s);
	}

  if (! name)
  {
	  s = MimeHeaders_get(hdrs, HEADER_CONTENT_TYPE, PR_FALSE, PR_FALSE);
	  if (s)
	  {
		  name = MimeHeaders_get_parameter(s, HEADER_PARM_NAME, NULL, NULL);
		  PR_Free(s);
	  }
  }

  if (! name)
	  name = MimeHeaders_get (hdrs, HEADER_CONTENT_NAME, PR_FALSE, PR_FALSE);
  
  if (! name)
	  name = MimeHeaders_get (hdrs, HEADER_X_SUN_DATA_NAME, PR_FALSE, PR_FALSE);

  if (name)
  {
		/*	First remove continuation delimiters (CR+LF+space), then
			remove escape ('\\') characters, then attempt to decode
			mime-2 encoded-words. The latter two are done in 
			mime_decode_filename. 
			*/
		MIME_StripContinuations(name);

		/*	Argh. What we should do if we want to be robust is to decode qtext
			in all appropriate headers. Unfortunately, that would be too scary
			at this juncture. So just decode qtext/mime2 here. */
	   	cvt = mime_decode_filename(name);
	   	if (cvt && cvt != name)
	   	{
	   		PR_Free(name);
	   		name = cvt;
	   	}
  }

  return name;
}

#ifdef XP_UNIX
/* This piece of junk is so that I can use BBDB with Mozilla.
   = Put bbdb-srv.perl on your path.
   = Put bbdb-srv.el on your lisp path.
   = Make sure gnudoit (comes with xemacs) is on your path.
   = Put (gnuserv-start) in ~/.emacs
   = setenv NS_MSG_DISPLAY_HOOK bbdb-srv.perl
 */
void
MimeHeaders_do_unix_display_hook_hack(MimeHeaders *hdrs)
{
  static char *cmd = 0;
  if (!cmd)
  {
  /* The first time we're invoked, look up the command in the
    environment.  Use "" as the `no command' tag. */
    cmd = getenv("NS_MSG_DISPLAY_HOOK");
    if (!cmd)
      cmd = "";
    else
      cmd = PL_strdup(cmd);
  }
  
  /* Invoke "cmd" at the end of a pipe, and give it the headers on stdin.
	 The command is expected to be safe from hostile input!!
  */
  if (cmd && *cmd)
  {
    FILE *fp = popen(cmd, "w");
    if (fp)
    {
      fwrite(hdrs->all_headers, 1, hdrs->all_headers_fp, fp);
      pclose(fp);
    }
  }
}
#endif /* XP_UNIX */

static void
MimeHeaders_compact (MimeHeaders *hdrs)
{
  PR_ASSERT(hdrs);
  if (!hdrs) return;

  PR_FREEIF(hdrs->obuffer);
  hdrs->obuffer_fp = 0;
  hdrs->obuffer_size = 0;

  /* These really shouldn't have gotten out of whack again. */
  PR_ASSERT(hdrs->all_headers_fp <= hdrs->all_headers_size &&
			hdrs->all_headers_fp + 100 > hdrs->all_headers_size);
}

/* Writes the headers as text/plain.
   This writes out a blank line after the headers, unless
   dont_write_content_type is true, in which case the header-block
   is not closed off, and none of the Content- headers are written.
 */
int
MimeHeaders_write_raw_headers (MimeHeaders *hdrs, MimeDisplayOptions *opt,
							   PRBool dont_write_content_type)
{
  int status;

  if (hdrs && !hdrs->done_p)
	{
	  hdrs->done_p = PR_TRUE;
	  status = MimeHeaders_build_heads_list(hdrs);
	  if (status < 0) return 0;
	}

  if (!dont_write_content_type)
	{
	  char nl[] = MSG_LINEBREAK;
	  if (hdrs)
		{
		  status = MimeHeaders_write(opt, hdrs->all_headers,
									 hdrs->all_headers_fp);
		  if (status < 0) return status;
		}
	  status = MimeHeaders_write(opt, nl, PL_strlen(nl));
	  if (status < 0) return status;
	}
  else if (hdrs)
	{
	  PRInt32 i;
	  for (i = 0; i < hdrs->heads_size; i++)
		{
		  char *head = hdrs->heads[i];
		  char *end = (i == hdrs->heads_size-1
					   ? hdrs->all_headers + hdrs->all_headers_fp
					   : hdrs->heads[i+1]);

		  PR_ASSERT(head);
		  if (!head) continue;

		  /* Don't write out any Content- header. */
		  if (!PL_strncasecmp(head, "Content-", 8))
			continue;

		  /* Write out this (possibly multi-line) header. */
		  status = MimeHeaders_write(opt, head, end - head);
		  if (status < 0) return status;
		}
	}

  if (hdrs)
  	MimeHeaders_compact (hdrs);

  return 0;
}
