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

#include "nsMsgMessageFlags.h"
#include "mimerosetta.h"
#include "mimei.h"
#include "xpgetstr.h"
#include "libi18n.h"
#include "modmime.h"

#include "prmem.h"
#include "plstr.h"
#include "msgcom.h"
#include "imap.h"
#include "prefapi.h"
#include "mimebuf.h"
#include "plugin_inst.h"
#include "mimemoz2.h"
#include "nsIMimeEmitter.h"
#include "nsIPref.h"

extern "C" int MK_OUT_OF_MEMORY;
extern "C" int MK_MSG_NO_HEADERS;
extern "C" int MK_MSG_MIME_MAC_FILE;
extern "C" int MK_MSG_IN_MSG_X_USER_WROTE;

extern "C" int MK_MSG_USER_WROTE;
extern "C" int MK_MSG_UNSPECIFIED_TYPE;
extern "C" int MK_MSG_XSENDER_INTERNAL;
extern "C" int MK_MSG_ADDBOOK_MOUSEOVER_TEXT;
extern "C" int MK_MSG_SHOW_ATTACHMENT_PANE;

extern "C" int MK_MIMEHTML_DISP_SUBJECT;
extern "C" int MK_MIMEHTML_DISP_RESENT_COMMENTS;
extern "C" int MK_MIMEHTML_DISP_RESENT_DATE;
extern "C" int MK_MIMEHTML_DISP_RESENT_SENDER;
extern "C" int MK_MIMEHTML_DISP_RESENT_FROM;
extern "C" int MK_MIMEHTML_DISP_RESENT_TO;
extern "C" int MK_MIMEHTML_DISP_RESENT_CC;
extern "C" int MK_MIMEHTML_DISP_DATE;
extern "C" int MK_MIMEHTML_DISP_SUBJECT;
extern "C" int MK_MIMEHTML_DISP_SENDER;
extern "C" int MK_MIMEHTML_DISP_FROM;
extern "C" int MK_MIMEHTML_DISP_REPLY_TO;
extern "C" int MK_MIMEHTML_DISP_ORGANIZATION;
extern "C" int MK_MIMEHTML_DISP_TO;
extern "C" int MK_MIMEHTML_DISP_CC;
extern "C" int MK_MIMEHTML_DISP_NEWSGROUPS;
extern "C" int MK_MIMEHTML_DISP_FOLLOWUP_TO;
extern "C" int MK_MIMEHTML_DISP_REFERENCES;
extern "C" int MK_MIMEHTML_DISP_MESSAGE_ID;
extern "C" int MK_MIMEHTML_DISP_RESENT_MESSAGE_ID;
extern "C" int MK_MIMEHTML_DISP_BCC;
extern "C" int MK_MIMEHTML_SHOW_SECURITY_ADVISOR;
extern "C" int MK_MIMEHTML_VERIFY_SIGNATURE;
extern "C" int MK_MIMEHTML_DOWNLOAD_STATUS_HEADER;
extern "C" int MK_MIMEHTML_DOWNLOAD_STATUS_NOT_DOWNLOADED;

extern "C" int MK_MIMEHTML_ENC_AND_SIGNED;
extern "C" int MK_MIMEHTML_SIGNED;
extern "C" int MK_MIMEHTML_XLATED;
extern "C" int MK_MIMEHTML_CERTIFICATES;
extern "C" int MK_MIMEHTML_ENC_SIGNED_BAD;
extern "C" int MK_MIMEHTML_SIGNED_BAD;
extern "C" int MK_MIMEHTML_XLATED_BAD;
extern "C" int MK_MIMEHTML_CERTIFICATES_BAD;
extern "C" int MK_MIMEHTML_SIGNED_UNVERIFIED;

#ifdef RICHIE
extern "C" int MK_MSG_SHOW_ALL_RECIPIENTS;
extern "C" int MK_MSG_SHOW_SHORT_RECIPIENTS;
#endif /* RICHIE */

extern "C" char *MIME_DecodeMimePartIIStr(const char *header, char *charset);

extern "C" 
char *
MIME_StripContinuations(char *original);


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
	  XP_MEMCPY(hdrs2->all_headers, hdrs->all_headers, hdrs->all_headers_fp);

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

/* Discard the buffer, when we probably won't be needing it any more. */
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


int MimeHeaders_build_heads_list(MimeHeaders *hdrs);

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
  XP_MEMCPY(hdrs->all_headers+hdrs->all_headers_fp, buffer, size);
  hdrs->all_headers_fp += size;

  return 0;
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
	  char *s = (char *)PR_Realloc(hdrs->all_headers, hdrs->all_headers_fp);
	  if (s) /* can this ever fail?  we're making it smaller... */
		{
		  hdrs->all_headers = s;  /* in case it got relocated */
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
	return MK_OUT_OF_MEMORY;
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
	  for (; colon > head && XP_IS_SPACE(colon[-1]); colon--)
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
		while (contents <= end && XP_IS_SPACE(*contents))
		  contents++;

		/* If we're supposed to strip at the frist token, pull `end' back to
		   the first whitespace or ';' after the first token.
		 */
		if (strip_p)
		  {
			for (s = contents;
				 s <= end && *s != ';' && *s != ',' && !XP_IS_SPACE(*s);
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
			*s++ = LINEBREAK[0];
# if (LINEBREAK_LEN == 2)
			*s++ = LINEBREAK[1];
# endif
			*s++ = '\t';
		  }

		/* Take off trailing whitespace... */
		while (end > contents && XP_IS_SPACE(end[-1]))
		  end--;

		if (end > contents)
		  {
		    /* Now copy the header's contents in...
		     */
		    XP_MEMCPY(s, contents, end - contents);
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
  for (; *str && XP_IS_SPACE(*str); str++)
	;
  if (!*str)
	return 0;

  while (*str)
	{
	  const char *token_start = str;
	  const char *token_end = 0;
	  const char *value_start = str;
	  const char *value_end = 0;

	  PR_ASSERT(!XP_IS_SPACE(*str)); /* should be after whitespace already */

	  /* Skip forward to the end of this token. */
	  for (; *str && !XP_IS_SPACE(*str) && *str != '=' && *str != ';'; str++)
		;
	  token_end = str;

	  /* Skip over whitespace, '=', and whitespace */
	  while (XP_IS_SPACE (*str)) str++;
	  if (*str == '=') str++;
	  while (XP_IS_SPACE (*str)) str++;

	  if (*str != '"')
		{
		  /* The value is a token, not a quoted string. */
		  value_start = str;
		  for (value_end = str;
			   *value_end && !XP_IS_SPACE (*value_end) && *value_end != ';';
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
		  if (! s) return 0;  /* MK_OUT_OF_MEMORY */
		  XP_MEMCPY (s, value_start, value_end - value_start);
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
					  XP_MEMCPY(*charset, value_start, s_quote1-value_start);
					  *(*charset+(s_quote1-value_start)) = 0;
				  }
			  }
			  if (language && s_quote1 && s_quote2 && s_quote2 > s_quote1+1 &&
				  s_quote2 < value_end)
			  {
				  *language = (char *) PR_MALLOC(s_quote2-(s_quote1+1)+1);
				  if (*language)
				  {
					  XP_MEMCPY(*language, s_quote1+1, s_quote2-(s_quote1+1));
					  *(*language+(s_quote2-(s_quote1+1))) = 0;
				  }
			  }
			  if (s_quote2 && s_quote2+1 < value_end)
			  {
				  PR_ASSERT(!s);
				  s = (char *) PR_MALLOC(value_end-(s_quote2+1)+1);
				  if (s)
				  {
					  XP_MEMCPY(s, s_quote2+1, value_end-(s_quote2+1));
					  *(s+(value_end-(s_quote2+1))) = 0;
					  if (needUnescape)
					  {
						  NET_UnEscape(s);
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
				  XP_MEMCPY(s+len, value_start, value_end-value_start);
				  *(s+len+(value_end-value_start)) = 0;
				  if (needUnescape)
					  NET_UnEscape(s+len);
			  }
		  }
	  }

	  /* str now points after the end of the value.
		 skip over whitespace, ';', whitespace. */
	  while (XP_IS_SPACE (*str)) str++;
	  if (*str == ';') str++;
	  while (XP_IS_SPACE (*str)) str++;
	}
  return s;
}


#define MimeHeaders_write(OPT,DATA,LENGTH) \
	MimeOptions_write((OPT), (DATA), (LENGTH), PR_TRUE);

static char *
MimeHeaders_default_news_link_generator (const char *dest, void *closure,
										 MimeHeaders *headers)
{
  /* This works as both the generate_news_url_fn and as the
	 generate_reference_url_fn. */
  char *prefix = "news:";
  char *new_dest = NET_Escape (dest, URL_XALPHAS);
  char *result = (char *) PR_MALLOC (PL_strlen (new_dest) +
									PL_strlen (prefix) + 1);
  if (result)
	{
	  PL_strcpy (result, prefix);
	  PL_strcat (result, new_dest);
	}
  PR_FREEIF (new_dest);
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
  int num = MSG_ParseRFC822Addresses(dest, &names, &addresses);
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
	MSG_UnquotePhraseOrAddr (addr, &unquotedAddr);
	if (!unquotedAddr) 
		continue;
	if (name)
		MSG_UnquotePhraseOrAddr (name, &unquotedName);

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
			char *localizedPart = PR_smprintf (XP_GetString (MK_MSG_ADDBOOK_MOUSEOVER_TEXT), jsSafeName);
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
	tmp2 = NET_Escape(tmp, URL_XALPHAS);
	PR_Free(tmp);
	if (!tmp2) break;
	result = PR_smprintf("addbook:add?vcard=%s", tmp2);
	PR_Free(tmp2);
	break;
  }
  PR_FREEIF(names);
  PR_FREEIF(addresses);
  return result;
}

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
	  const char *output_charset = (opt->override_charset
									? opt->override_charset
									: opt->default_charset);
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



static const char *
MimeHeaders_localize_header_name(const char *name, MimeDisplayOptions *opt)
{
  const char *new_name = 0;
  PRInt16 output_csid = 0;
  int display_key =  (!PL_strcasecmp(name,HEADER_BCC)			     ? MK_MIMEHTML_DISP_BCC :
                      !PL_strcasecmp(name,HEADER_CC)			     ? MK_MIMEHTML_DISP_CC :
                      !PL_strcasecmp(name,HEADER_DATE)			   ? MK_MIMEHTML_DISP_DATE :
                      !PL_strcasecmp(name,HEADER_FOLLOWUP_TO)	 ? MK_MIMEHTML_DISP_FOLLOWUP_TO :
                      !PL_strcasecmp(name,HEADER_FROM)			   ? MK_MIMEHTML_DISP_FROM :
                      !PL_strcasecmp(name,HEADER_MESSAGE_ID)	 ? MK_MIMEHTML_DISP_MESSAGE_ID :
                      !PL_strcasecmp(name,HEADER_NEWSGROUPS)	 ? MK_MIMEHTML_DISP_NEWSGROUPS :
                      !PL_strcasecmp(name,HEADER_ORGANIZATION) ? MK_MIMEHTML_DISP_ORGANIZATION :
                      !PL_strcasecmp(name,HEADER_REFERENCES)	 ? MK_MIMEHTML_DISP_REFERENCES :
                      !PL_strcasecmp(name,HEADER_REPLY_TO)		 ? MK_MIMEHTML_DISP_REPLY_TO :
                      !PL_strcasecmp(name,HEADER_RESENT_CC)		 ? MK_MIMEHTML_DISP_RESENT_CC :
                      !PL_strcasecmp(name,HEADER_RESENT_COMMENTS)
                                    ? MK_MIMEHTML_DISP_RESENT_COMMENTS :
                      !PL_strcasecmp(name,HEADER_RESENT_DATE)	 ? MK_MIMEHTML_DISP_RESENT_DATE :
                      !PL_strcasecmp(name,HEADER_RESENT_FROM)	 ? MK_MIMEHTML_DISP_RESENT_FROM :
                      !PL_strcasecmp(name,HEADER_RESENT_MESSAGE_ID)
                                    ? MK_MIMEHTML_DISP_RESENT_MESSAGE_ID :
                      !PL_strcasecmp(name,HEADER_RESENT_SENDER)? MK_MIMEHTML_DISP_RESENT_SENDER :
                      !PL_strcasecmp(name,HEADER_RESENT_TO)		 ? MK_MIMEHTML_DISP_RESENT_TO :
                      !PL_strcasecmp(name,HEADER_SENDER)		   ? MK_MIMEHTML_DISP_SENDER :
                      !PL_strcasecmp(name,HEADER_SUBJECT)		   ? MK_MIMEHTML_DISP_SUBJECT :
                      !PL_strcasecmp(name,HEADER_TO)			     ? MK_MIMEHTML_DISP_TO :
                      !PL_strcasecmp(name,HEADER_SUBJECT)		   ? MK_MIMEHTML_DISP_SUBJECT :
                      0);
  
  if (!display_key)
    return name;
  
  output_csid = INTL_CharSetNameToID((opt->override_charset
                                      ? opt->override_charset
                                      : opt->default_charset));
  new_name = XP_GetStringForHTML(display_key, output_csid, (char*)name);  
  if (new_name)
    return new_name;
  else
    return name;
}


static int
MimeHeaders_write_random_header_1 (MimeHeaders *hdrs,
								   const char *name, const char *contents,
								   MimeDisplayOptions *opt,
								   PRBool subject_p)
{
  int status = 0;
  PRInt32 contents_length;
  char *out;
  char *converted = 0;
  PRInt32 converted_length = 0;
  char    *cleanName = NULL;  /* To make sure there is no nasty HTML/JavaScript in Headers */

  PR_ASSERT(hdrs);
  if (!hdrs) return -1;

  if (!contents && subject_p)
	contents = "";
  else if (!contents)
	return 0;

  /* create a "clean" escaped header name for any embedded 
     JavaScript (if we have to) */
  if (name)
  {
    cleanName = NET_EscapeHTML(name);
    if (!cleanName)
      return MK_OUT_OF_MEMORY;
    else
      name = cleanName;
  }

  name = MimeHeaders_localize_header_name(name, opt);

  contents_length = PL_strlen(contents);
  status = MimeHeaders_convert_rfc1522(opt, contents, contents_length,
									   &converted, &converted_length);
  if (status < 0) 
  {
    PR_FREEIF(cleanName);
    return status;
  }
  if (converted)
	{
	  contents = converted;
	  contents_length = converted_length;
	}

  status = MimeHeaders_grow_obuffer (hdrs, PL_strlen (name) + 100 +
									 (contents_length * 4));
  if (status < 0)
	{
	  PR_FREEIF(converted);
    PR_FREEIF(cleanName);
	  return status;
	}

  out = hdrs->obuffer;

  if (subject_p && contents)
	{
	  PR_ASSERT(hdrs->munged_subject == NULL);
	  hdrs->munged_subject = PL_strdup(contents);
	}

  if (opt->fancy_headers_p)
	{
	  PL_strcpy (out, "<TR><TH VALIGN=BASELINE ALIGN=RIGHT NOWRAP>"); out += PL_strlen (out);
	  PL_strcpy (out, name); out += PL_strlen (out);

#define HEADER_MIDDLE_JUNK       ": </TH><TD>"

	  PL_strcpy (out, HEADER_MIDDLE_JUNK); out += PL_strlen (out);
	  if (subject_p)
		{
		  PL_strcpy (out, "<B>"); out += PL_strlen (out);
		}

	  /* Copy `contents' to `out', quoting HTML along the way.
		 Note: this function does no charset conversion; that has
		 already been done.
	   */
	  status = NET_ScanForURLs (
								NULL,
								contents, contents_length, out,
								hdrs->obuffer_size - (out - hdrs->obuffer) -10,
								PR_TRUE);
	  if (status < 0) return status;
	  out += PL_strlen(out);
	  PR_ASSERT(out < (hdrs->obuffer + hdrs->obuffer_size));

	  PL_strcpy (out, "</TD></TR>"); out += PL_strlen (out);
	}
  else
	{
	  /* The non-fancy version (no tables): for converting to plain text. */
	  char *s = NET_EscapeHTML (contents);
	  if (s)
		{
		  char *start, *end, *data_end;
		  PL_strcpy (out, "<NOBR><B>"); out += PL_strlen (out);
		  PL_strcpy (out, name); out += PL_strlen (out);
		  PL_strcpy (out, ": </B>"); out += PL_strlen (out);

		  data_end = s + PL_strlen (s);
		  start = s;
		  end = start;
		  while (end < data_end)
			for (end = start; end < data_end; end++)
			  if (*end == CR || *end == LF)
				{
				  XP_MEMCPY (out, start, end - start); out += (end - start);
				  PL_strcpy (out, "<BR>&nbsp;&nbsp;&nbsp;&nbsp;");
				  out += PL_strlen (out);
				  if (*end == CR && end < data_end && end[1] == LF)
					end++;
				  start = end + 1;
				}
		  if (start < end)
			{
			  XP_MEMCPY (out, start, end - start); out += (end - start);
			}
		  PL_strcpy (out, "</NOBR><BR>"); out += PL_strlen (out);
		  PR_Free (s);
		}
	}
  *out = 0; 

  status = MimeHeaders_write(opt, hdrs->obuffer, out - hdrs->obuffer);
  PR_FREEIF(converted);
  PR_FREEIF(cleanName);
  if (status < 0)
	return status;
  else
	return 1;
}


static int
MimeHeaders_write_random_header (MimeHeaders *hdrs, const char *name,
								 PRBool all_p, MimeDisplayOptions *opt)
{
  int status = 0;
  char *contents = MimeHeaders_get (hdrs, name, PR_FALSE, all_p);
  if (!contents) return 0;
  status = MimeHeaders_write_random_header_1(hdrs, name, contents, opt, PR_FALSE);
  PR_Free(contents);
  return status;
}

static int
MimeHeaders_write_subject_header_1 (MimeHeaders *hdrs, const char *name,
									const char *contents,
									MimeDisplayOptions *opt)
{
  return MimeHeaders_write_random_header_1(hdrs, name, contents, opt, PR_TRUE);
}

static int
MimeHeaders_write_subject_header (MimeHeaders *hdrs, const char *name,
								  MimeDisplayOptions *opt)
{
  int status = 0;
  char *contents = MimeHeaders_get (hdrs, name, PR_FALSE, PR_FALSE);
  status = MimeHeaders_write_subject_header_1 (hdrs, name, contents, opt);
  PR_FREEIF(contents);
  return status;
}

static int
MimeHeaders_write_grouped_header_1 (MimeHeaders *hdrs, const char *name,
									const char *contents,
									MimeDisplayOptions *opt,
									PRBool mail_header_p)
{
  static PRInt32       nHeaderDisplayLen = 15;  /* rhp: for new header wrap functionality */
  nsIPref *pref = GetPrefServiceManager(opt);   // Pref service manager

  int status = 0;
  PRInt32 contents_length;
  char *converted = 0;
  const char *orig_name = name;
  char *out;

  PR_ASSERT(hdrs);
  if (!hdrs) return -1;

  if (!contents)
	return 0;

  if (!opt->fancy_headers_p)
	return MimeHeaders_write_random_header (hdrs, name, PR_TRUE, opt);

  if (name) name = MimeHeaders_localize_header_name(name, opt);

  contents_length = PL_strlen(contents);

  if (mail_header_p)
	{
	  PRInt32 converted_length = 0;
	  status = MimeHeaders_convert_rfc1522(opt, contents, contents_length,
										   &converted, &converted_length);
	  if (status < 0) return status;
	  if (converted)
		{
		  contents = converted;
		  contents_length = converted_length;
		}
	}

  /* grow obuffer with potential XSENDER AUTH string */
  status = MimeHeaders_grow_obuffer (hdrs, (name ? PL_strlen (name) : 0)
									 + 100 + 2 *
									 PL_strlen(XP_GetString(MK_MSG_XSENDER_INTERNAL)) +
									 (contents_length * 4));
  if (status < 0) goto FAIL;

  out = hdrs->obuffer;
  if (name) {
	  PL_strcpy (out, "<TR><TH VALIGN=BASELINE ALIGN=RIGHT NOWRAP>"); out += PL_strlen (out);
	  PL_strcpy (out, name); out += PL_strlen (out);
	  PL_strcpy (out, ": </TH><TD>"); out += PL_strlen (out);
  }

  status = MimeHeaders_write(opt, hdrs->obuffer, out - hdrs->obuffer);
  if (status < 0) goto FAIL;

  // Short header pref
  if (pref)
    pref->GetIntPref("mailnews.max_header_display_length", &nHeaderDisplayLen);

  {
  int        headerCount = 0;
	const char *data_end = contents + contents_length;
	const char *last_end = 0;
	const char *this_start = contents;
	const char *this_end = this_start;
  while (this_end < data_end)
  {
    PRInt32 paren_depth = 0;
    
    /* Skip over whitespace and commas. */
    while (this_start < data_end &&
			   (XP_IS_SPACE (*this_start) || *this_start == ','))
         this_start++;
    
    this_end = this_start;
    
    /* Skip to the end, or the next comma, taking quoted strings,
    comments, and backslashes into account. */
    while (this_end < data_end &&
			   !(*this_end == ',' && paren_depth <= 0))
		  {
      if (*this_end == '\\')
        this_end++;
      else if (*this_end == '"')
      {
        this_end++;
        while (this_end < data_end && *this_end != '"')
        {
          if (*this_end == '\\')
            this_end++;
          this_end++;
        }
      }
      
      if (*this_end == '(')
        paren_depth++;
      else if (*this_end == ')' && paren_depth > 0)
        paren_depth--;
      
      this_end++;
		  }
    
    /* Push out the preceeding comma, whitespace, etc. */
    if (last_end && last_end != this_start)
		  {
      char *s;
      PL_strcpy (hdrs->obuffer, "</NOBR>");
      status = MimeHeaders_write(opt, hdrs->obuffer,
        PL_strlen(hdrs->obuffer));
      if (status < 0) goto FAIL;
      
      XP_MEMCPY (hdrs->obuffer, last_end, this_start - last_end);
      hdrs->obuffer [this_start - last_end] = 0;
      s = NET_EscapeHTML (hdrs->obuffer);
      if (!s)
      {
        status = MK_OUT_OF_MEMORY;
        goto FAIL;
      }
      status = MimeHeaders_write(opt, s, PL_strlen (s));
      PR_Free (s);
      if (status < 0) goto FAIL;
      
      /* Emit a space, to make sure long newsgroup lines, or any
      header line with no spaces after the commas, wrap. */   
      hdrs->obuffer[0] = ' ';
      hdrs->obuffer[1] = 0;
      status = MimeHeaders_write(opt, hdrs->obuffer, PL_strlen (hdrs->obuffer));
      if (status < 0) goto FAIL;
		  }
    
    /* Push out this address-or-newsgroup. */
    if (this_start != this_end)
		  {
      char *s;
      MimeHTMLGeneratorFunction fn = 0;
      void *arg;
      char *link, *link2;
      PRBool mail_p;
      char *extraAnchorText = NULL;
      
      XP_MEMCPY (hdrs->obuffer, this_start, this_end - this_start);
      hdrs->obuffer [this_end - this_start] = 0;
      s = NET_EscapeHTML (hdrs->obuffer);
      if (!s)
      {
        status = MK_OUT_OF_MEMORY;
        goto FAIL;
      }
      mail_p = (mail_header_p || !PL_strcasecmp ("poster", hdrs->obuffer));
      
      if (mail_p)
      {
        fn = opt->generate_mailto_url_fn;
        arg = opt->html_closure;
      }
      else
      {
        fn = opt->generate_news_url_fn;
        arg = opt->html_closure;
      }
      if (! fn)
      {
        if (mail_p)
        {
          fn = MimeHeaders_default_addbook_link_generator;
          arg = &extraAnchorText; 
        }
        else
        {
          fn = MimeHeaders_default_news_link_generator;
          arg = 0;
        }
      }
      
      if (*hdrs->obuffer && opt->fancy_links_p)
        link = fn (hdrs->obuffer, arg, hdrs);
      else
        link = 0;
      
      link2 = (link ? NET_EscapeHTML (link) : 0);
      PR_FREEIF (link);
      link = link2;
      
      if (link)
      {
        status = MimeHeaders_grow_obuffer (hdrs, PL_strlen (link) +
          /* This used to be 100. Extra 8 bytes counts for " PRIVATE" */
          PL_strlen (s) + 108);
        
        if (status < 0) goto FAIL;
      }
      out = hdrs->obuffer;
      if (link)
      {
        PL_strcpy (out, "<A HREF=\""); out += PL_strlen (out);
        PL_strcpy (out, link); out += PL_strlen (out);
        PL_strcpy (out, "\""); out += 1;
        if (extraAnchorText)
        {
          PL_strcpy (out, extraAnchorText);
          out += PL_strlen(out);
        }
        PL_strcpy (out, " PRIVATE>"); out += PL_strlen(out);
      }
      PL_strcpy (out, "<NOBR>"); out += PL_strlen (out);
      
      PL_strcpy (out, s); out += PL_strlen (out);
      
      PR_Free (s);
      if (link)
      {
        PL_strcpy (out, "</A>"); out += PL_strlen (out);
        PR_Free (link);
      }
      if (extraAnchorText)
        PR_Free (extraAnchorText);

      /* Begin hack of out of envelope XSENDER info */
      if (orig_name && !PL_strcasecmp(orig_name, HEADER_FROM)) { 
        char * statusLine = MimeHeaders_get (hdrs, HEADER_X_MOZILLA_STATUS, PR_FALSE, PR_FALSE);
        PRUint16 flags =0;
        
#define UNHEX(C) \
  ((C >= '0' && C <= '9') ? C - '0' : \
  ((C >= 'A' && C <= 'F') ? C - 'A' + 10 : \
        ((C >= 'a' && C <= 'f') ? C - 'a' + 10 : 0)))
        
        if (statusLine && PL_strlen(statusLine) == 4) {
          int i;
          char *cp = statusLine;
          for (i=0; i < 4; i++, cp++) {
            flags = (flags << 4) | UNHEX(*cp);
          }
          PR_FREEIF(statusLine);
        }
        
        if (flags & MSG_FLAG_SENDER_AUTHED) {
          PL_strcpy (out, XP_GetString(MK_MSG_XSENDER_INTERNAL));
          out += PL_strlen (out);
        }
      }
      /* End of XSENDER info */
      status = MimeHeaders_write(opt, hdrs->obuffer,
        PL_strlen (hdrs->obuffer));
      if (status < 0) goto FAIL;
      }
      
      /* now go around again */
      last_end = this_end;
      this_start = last_end;
      
      ++headerCount;
      if (this_end >= data_end)
      {
        /*  new toggle header functionality */
        if ((nHeaderDisplayLen > 0) &&
          (mail_header_p) && 
          (headerCount > nHeaderDisplayLen) &&
          ( (orig_name) && 
          (!PL_strcasecmp(orig_name, HEADER_CC)) || 
          (!PL_strcasecmp(orig_name, HEADER_BCC)) ||
          (!PL_strcasecmp(orig_name, HEADER_TO)) 
          ) 
          )
        {
          /* OK: We have reached the end of the headers! And */
          /* we will only be in here if we have hit the short header limit */
          /* If we do get here, then we should keep the short header bucket */
          /* around and make sure that it gets output to the XML file. */
          /* So for now, there is no code here since we don't have the buckets */
          /* implemented */
        }
        else
        {
          /* We should get rid of a short header bucket since we didn't hit */
          /* the limit! */
        }
        
        /* We are at the end */ 
        break;
      }
      
      /* rhp - this is new code for the expandable header display */
      if ((nHeaderDisplayLen > 0) &&
        (mail_header_p) && 
        (headerCount >= nHeaderDisplayLen) &&
        ( (orig_name) && 
        (!PL_strcasecmp(orig_name, HEADER_CC)) || 
        (!PL_strcasecmp(orig_name, HEADER_BCC)) ||
        (!PL_strcasecmp(orig_name, HEADER_TO)) 
        ) 
        )
      {
        /* WE ARE STILL PROCESSING HEADERS HERE!!! - BUT OVER SHORT THRESHOLD */
        /* Now this is the code that we will hit when the output of headers */
        /* is GREATER than the threshold value that the user has set. In this */
        /* case, we need to stop outputting to the short header bucket and  */
        /* keep sending the addresses to the long header bucket...so unlike  */
        /* before, we need to keep going and just manage the buckets */
        
        /* Not sure we need to do anything here except make sure that we don't
        output anything to the short header bucket! */
      }
      else
      {
      /* This is where we would have to dump stuff to the short header
        bucket even if we don't know we are going to keep it around later */
      }
      /* rhp - this is new code for the expandable header display */
    }

  } /* End of while loop - still processing headers */

  out = hdrs->obuffer;
  PL_strcpy (out, "</NOBR>"); out += PL_strlen (out);
  if (name) PL_strcpy (out, "</TD></TR>"); out += PL_strlen (out);
  *out = 0;
  status = MimeHeaders_write(opt, hdrs->obuffer, out - hdrs->obuffer);
  if (status < 0) goto FAIL;
  else status = 1;

 FAIL:
  PR_FREEIF(converted);
  return status;
}

static int
MimeHeaders_write_grouped_header (MimeHeaders *hdrs, const char *name,
								  MimeDisplayOptions *opt,
								  PRBool mail_header_p)
{
  int status = 0;
  char *contents = MimeHeaders_get (hdrs, name, PR_FALSE, PR_TRUE);
  if (!contents) return 0;
  status = MimeHeaders_write_grouped_header_1 (hdrs, name, contents, opt,
											   mail_header_p);
  PR_Free(contents);
  return status;
}

#define MimeHeaders_write_address_header(hdrs,name,opt) \
	MimeHeaders_write_grouped_header(hdrs,name,opt,PR_TRUE)
#define MimeHeaders_write_news_header(hdrs,name,opt) \
	MimeHeaders_write_grouped_header(hdrs,name,opt,PR_FALSE)

#define MimeHeaders_write_address_header_1(hdrs,name,contents,opt) \
	MimeHeaders_write_grouped_header_1(hdrs,name,contents,opt,PR_TRUE)
#define MimeHeaders_write_news_header_1(hdrs,name,contents,opt) \
	MimeHeaders_write_grouped_header_1(hdrs,name,contents,opt,PR_FALSE)


static int
MimeHeaders_write_id_header_1 (MimeHeaders *hdrs, const char *name,
							   const char *contents, PRBool show_ids,
							   MimeDisplayOptions *opt)
{
  int status = 0;
  PRInt32 contents_length;
  char *out;

  PR_ASSERT(hdrs);
  if (!hdrs) return -1;

  if (!contents)
	return 0;

  if (!opt->fancy_headers_p)
	return MimeHeaders_write_random_header (hdrs, name, PR_TRUE, opt);

  name = MimeHeaders_localize_header_name(name, opt);

  contents_length = PL_strlen(contents);
  status = MimeHeaders_grow_obuffer (hdrs, PL_strlen (name) + 100 +
									 (contents_length * 4));
  if (status < 0) goto FAIL;

  out = hdrs->obuffer;
  PL_strcpy (out, "<TR><TH VALIGN=BASELINE ALIGN=RIGHT NOWRAP>"); out += PL_strlen (out);
  PL_strcpy (out, name); out += PL_strlen (out);
  PL_strcpy (out, ": </TH><TD>"); out += PL_strlen (out);

  status = MimeHeaders_write(opt, hdrs->obuffer, out - hdrs->obuffer);
  if (status < 0) goto FAIL;

  {
	const char *data_end = contents + contents_length;
	const char *last_end = 0;
	const char *this_start = contents;
	const char *this_end = this_start;
	int id_count = 0;
	while (this_end < data_end)
	  {
		/* Skip over whitespace. */
		while (this_start < data_end && XP_IS_SPACE (*this_start))
		  this_start++;

		this_end = this_start;

		/* At this point *this_start should be '<', but if it's not,
		   there's not a lot we can do about it... */

		/* Skip to the end of the message ID, taking quoted strings and
		   backslashes into account. */
		while (this_end < data_end && *this_end != '>')
		  {
			if (*this_end == '\\')
			  this_end++;
			else if (*this_end == '"')
			  while (this_end < data_end && *this_end != '"')
				{
				  if (*this_end == '\\')
					this_end++;
				  this_end++;
				}
			this_end++;
		  }

		if (*this_end == '>')  /* If it's not, that's illegal. */
		  this_end++;

		/* Push out the preceeding whitespace. */
		if (last_end && last_end != this_start)
		  {
			char *s;
			XP_MEMCPY (hdrs->obuffer, last_end, this_start - last_end);
			hdrs->obuffer [this_start - last_end] = 0;
			s = NET_EscapeHTML (hdrs->obuffer);
			if (!s)
			  {
				status = MK_OUT_OF_MEMORY;
				goto FAIL;
			  }
			status = MimeHeaders_write(opt, s, PL_strlen (s));
			PR_Free (s);
			if (status < 0) goto FAIL;
		  }

		/* Push out this ID. */
		if (this_start != this_end)
		  {
			char *s;
			MimeHTMLGeneratorFunction fn = 0;
			void *arg;
			char *link;

			fn = opt->generate_reference_url_fn;
			arg = opt->html_closure;
			if (! fn)
			  {
				fn = MimeHeaders_default_news_link_generator;
				arg = 0;
			  }

			{
			  char *link2 = NULL;
			  const char *id = this_start;
			  PRInt32 size = this_end - this_start;
			  if (*id == '<')
				id++, size--;
			  XP_MEMCPY (hdrs->obuffer, id, size);
			  hdrs->obuffer [size] = 0;
			  if (hdrs->obuffer [size-1] == '>')
				hdrs->obuffer [size-1] = 0;

			  link = fn (hdrs->obuffer, arg, hdrs);
			  if (link)
				{
				  link2 = NET_EscapeHTML(link);
				  PR_Free(link);
				}
			  link = link2;
			}

			if (show_ids)
			  {
				XP_MEMCPY (hdrs->obuffer, this_start, this_end - this_start);
				hdrs->obuffer [this_end - this_start] = 0;
				s = NET_EscapeHTML (hdrs->obuffer);
				if (!s)
				  {
					status = MK_OUT_OF_MEMORY;
					goto FAIL;
				  }
			  }
			else
			  {
				char buf[50];
				XP_SPRINTF(buf, "%d", ++id_count);
				s = PL_strdup(buf);
				if (!s)
				  {
					status = MK_OUT_OF_MEMORY;
					goto FAIL;
				  }
			  }

			if (link)
			  {
				status = MimeHeaders_grow_obuffer (hdrs, PL_strlen (link) +
				/* This used to be 100. Extra 8 bytes counts for " PRIVATE" */
												   PL_strlen (s) + 108);
				if (status < 0) goto FAIL;
			  }
			out = hdrs->obuffer;

			if (!show_ids && id_count > 1)
			  {
				PL_strcpy (out, ", "); out += PL_strlen (out);
			  }
			  
			if (link)
			  {
				PL_strcpy (out, "<A HREF=\""); out += PL_strlen (out);
				PL_strcpy (out, link); out += PL_strlen (out);
				PL_strcpy (out, "\" PRIVATE>"); out += PL_strlen (out);
			  }
			PL_strcpy (out, s); out += PL_strlen (out);
			PR_Free (s);
			if (link)
			  {
				PL_strcpy (out, "</A>"); out += PL_strlen (out);
				PR_Free (link);
			  }
			status = MimeHeaders_write(opt, hdrs->obuffer,
									   PL_strlen (hdrs->obuffer));
			if (status < 0) goto FAIL;
		  }

		/* now go around again */
		last_end = this_end;
		this_start = last_end;
	  }
  }

  out = hdrs->obuffer;
  PL_strcpy (out, "</TD></TR>"); out += PL_strlen (out);
  *out = 0;
  status = MimeHeaders_write(opt, hdrs->obuffer, out - hdrs->obuffer);
  if (status < 0) goto FAIL;
  else status = 1;

 FAIL:
  return status;
}

static int
MimeHeaders_write_id_header (MimeHeaders *hdrs, const char *name,
							 PRBool show_ids, MimeDisplayOptions *opt)
{
  int status = 0;
  char *contents = NULL;
  
  PR_ASSERT(hdrs);
  if (!hdrs) return -1;

  contents = MimeHeaders_get (hdrs, name, PR_FALSE, PR_TRUE);
  if (!contents) return 0;
  status = MimeHeaders_write_id_header_1 (hdrs, name, contents, show_ids, opt);
  PR_Free(contents);
  return status;
}



int MimeHeaders_write_all_headers (MimeHeaders *, MimeDisplayOptions *, PRBool);
static int MimeHeaders_write_microscopic_headers (MimeHeaders *,
												  MimeDisplayOptions *);
static int MimeHeaders_write_citation_headers (MimeHeaders *,
											   MimeDisplayOptions *);


/* This list specifies the headers shown in the "normal" headers display mode,
   and also specifies the preferred ordering.  Headers which do not occur are
   not shown (except Subject) and certain header-pairs have some special
   behavior (see comments.)

  #### We should come up with some secret way to set this from a user 
  preference, perhaps from a special file in the ~/.netscape/ directory.
 */
static const char *mime_interesting_headers[] = {
  HEADER_SUBJECT,
  HEADER_RESENT_COMMENTS,
  HEADER_RESENT_DATE,
  HEADER_RESENT_SENDER,		/* not shown if HEADER_RESENT_FROM is present */
  HEADER_RESENT_FROM,
  HEADER_RESENT_TO,
  HEADER_RESENT_CC,
  HEADER_DATE,
  HEADER_SENDER,			/* not shown if HEADER_FROM is present */
  HEADER_FROM,
  HEADER_REPLY_TO,			/* not shown if HEADER_FROM has the same addr. */
  HEADER_ORGANIZATION,
  HEADER_TO,
  HEADER_CC,
  HEADER_BCC,
  HEADER_NEWSGROUPS,
  HEADER_FOLLOWUP_TO,
  HEADER_REFERENCES,		/* not shown in MimeHeadersSomeNoRef mode. */

#ifdef DEBUG_jwz
  HEADER_X_MAILER,			/* jwz finds these useful for debugging... */
  HEADER_X_NEWSREADER,
  HEADER_X_POSTING_SOFTWARE,
#endif
  0
};


static int
MimeHeaders_write_interesting_headers (MimeHeaders *hdrs,
									   MimeDisplayOptions *opt)
{
  PRBool wrote_any_p = PR_FALSE;
  int status = 0;
  const char **rest;
  PRBool did_from = PR_FALSE;
  PRBool did_resent_from = PR_FALSE;

  PR_ASSERT(hdrs);
  if (!hdrs) return -1;

  status = MimeHeaders_grow_obuffer (hdrs, 200);
  if (status < 0) return status;

  for (rest = mime_interesting_headers; *rest; rest++)
	{
	  const char *name = *rest;

	  /* The Subject header gets written in bold.
	   */
	  if (!PL_strcasecmp(name, HEADER_SUBJECT))
		{
		  status = MimeHeaders_write_subject_header (hdrs, name, opt);
		}

	  /* The References header is never interesting if we're emitting paper,
		 since you can't click on it.
	   */
	  else if (!PL_strcasecmp(name, HEADER_REFERENCES))
		{
		  if (opt->headers != MimeHeadersSomeNoRef)
			status = MimeHeaders_write_id_header (hdrs, name, PR_FALSE, opt);
		}

	  /* Random other Message-ID headers.  These aren't shown by default, but
		 if the user adds them to the list, display them clickably.
	   */
	  else if (!PL_strcasecmp(name, HEADER_MESSAGE_ID) ||
			   !PL_strcasecmp(name, HEADER_RESENT_MESSAGE_ID))
		{
		  status = MimeHeaders_write_id_header (hdrs, name, PR_TRUE, opt);
		}

	  /* The From header supercedes the Sender header.
	   */
	  else if (!PL_strcasecmp(name, HEADER_SENDER) ||
			   !PL_strcasecmp(name, HEADER_FROM))
		{
		  if (did_from) continue;
		  did_from = PR_TRUE;
		  status = MimeHeaders_write_address_header (hdrs, HEADER_FROM, opt);
		  if (status == 0)
			status = MimeHeaders_write_address_header (hdrs, HEADER_SENDER,
													   opt);
		}

	  /* Likewise, the Resent-From header supercedes the Resent-Sender header.
	   */
	  else if (!PL_strcasecmp(name, HEADER_RESENT_SENDER) ||
			   !PL_strcasecmp(name, HEADER_RESENT_FROM))
		{
		  if (did_resent_from) continue;
		  did_resent_from = PR_TRUE;
		  status = MimeHeaders_write_address_header (hdrs, HEADER_RESENT_FROM,
													 opt);
		  if (status == 0)
			status = MimeHeaders_write_address_header (hdrs,
													   HEADER_RESENT_SENDER,
													   opt);
		}

	  /* Emit the Reply-To header only if it differs from the From header.
		 (we just compare the `address' part.)
	   */
	  else if (!PL_strcasecmp(name, HEADER_REPLY_TO))
		{
		  char *reply_to = MimeHeaders_get (hdrs, HEADER_REPLY_TO,
											PR_FALSE, PR_FALSE);
		  if (reply_to)
			{
			  char *from = MimeHeaders_get (hdrs, HEADER_FROM, PR_FALSE, PR_FALSE);
			  char *froma = (from
							 ? MSG_ExtractRFC822AddressMailboxes(from)
							 : 0);
			  char *repa  = ((reply_to && froma)
							 ? MSG_ExtractRFC822AddressMailboxes(reply_to)
							 : 0);

			  PR_FREEIF(reply_to);
			  if (!froma || !repa || PL_strcasecmp (froma, repa))
				{
				  PR_FREEIF(froma);
				  PR_FREEIF(repa);
				  status = MimeHeaders_write_address_header (hdrs,
															 HEADER_REPLY_TO,
															 opt);
				}
			  PR_FREEIF(repa);
			  PR_FREEIF(froma);
			  PR_FREEIF(from);
			}
		  PR_FREEIF(reply_to);
		}

	  /* Random other address headers.
		 These headers combine all occurences: that is, if there is more than
		 one CC field, all of them will be combined and presented as one.
	   */
	  else if (!PL_strcasecmp(name, HEADER_RESENT_TO) ||
			   !PL_strcasecmp(name, HEADER_RESENT_CC) ||
			   !PL_strcasecmp(name, HEADER_TO) ||
			   !PL_strcasecmp(name, HEADER_CC) ||
			   !PL_strcasecmp(name, HEADER_BCC))
		{
		  status = MimeHeaders_write_address_header (hdrs, name, opt);
		}

	  /* Random other newsgroup headers.
		 These headers combine all occurences, as with address headers.
	   */
	  else if (!PL_strcasecmp(name, HEADER_NEWSGROUPS) ||
			   !PL_strcasecmp(name, HEADER_FOLLOWUP_TO))
		{
		  status = MimeHeaders_write_news_header (hdrs, name, opt);
		}

	  /* Everything else.
		 These headers don't combine: only the first instance of the header
		 will be shown, if there is more than one.
	   */
	  else
		{
		  status = MimeHeaders_write_random_header (hdrs, name, PR_FALSE, opt);
		}

	  if (status < 0) break;
	  wrote_any_p |= (status > 0);
	}

  return (status < 0 ? status : (wrote_any_p ? 1 : 0));
}


int
MimeHeaders_write_all_headers (MimeHeaders *hdrs, MimeDisplayOptions *opt, PRBool attachment)
{
  int status = 0;
  int i;
  PRBool wrote_any_p = PR_FALSE;

  nsIMimeEmitter   *mimeEmitter = GetMimeEmitter(opt);

  // No emitter, no point.
  if (!mimeEmitter)
    return -1;

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
      colon = head + 4;
      contents = colon + 1;
    }
    else
    {
      /* Find the colon. */
      for (colon = head; colon < end; colon++)
        if (*colon == ':') break;
        
        if (colon >= end) continue;   /* junk */
        
        /* Back up over whitespace before the colon. */
        ocolon = colon;
        for (; colon > head && XP_IS_SPACE(colon[-1]); colon--)
          ;
        
        contents = ocolon + 1;
    }
    
    /* Skip over whitespace after colon. */
    while (contents <= end && XP_IS_SPACE(*contents))
      contents++;
    
    /* Take off trailing whitespace... */
    while (end > contents && XP_IS_SPACE(end[-1]))
      end--;
    
    name = (char *)PR_MALLOC(colon - head + 1);
    if (!name) return MK_OUT_OF_MEMORY;
    XP_MEMCPY(name, head, colon - head);
    name[colon - head] = 0;
    
    c2 = (char *)PR_MALLOC(end - contents + 1);
    if (!c2)
    {
      PR_Free(name);
      return MK_OUT_OF_MEMORY;
    }
    XP_MEMCPY(c2, contents, end - contents);
    c2[end - contents] = 0;
    
    /*****************************************
    if (!PL_strcasecmp(name, HEADER_CC) ||
    !PL_strcasecmp(name, HEADER_FROM) ||
    !PL_strcasecmp(name, HEADER_REPLY_TO) ||
    !PL_strcasecmp(name, HEADER_RESENT_CC) ||
    !PL_strcasecmp(name, HEADER_RESENT_FROM) ||
    !PL_strcasecmp(name, HEADER_RESENT_SENDER) ||
    !PL_strcasecmp(name, HEADER_RESENT_TO) ||
    !PL_strcasecmp(name, HEADER_SENDER) ||
    !PL_strcasecmp(name, HEADER_TO))
    status = MimeHeaders_write_address_header_1(hdrs, name, c2, opt);
    else if (!PL_strcasecmp(name, HEADER_FOLLOWUP_TO) ||
    !PL_strcasecmp(name, HEADER_NEWSGROUPS))
    status = MimeHeaders_write_news_header_1(hdrs, name, c2, opt);
    else if (!PL_strcasecmp(name, HEADER_MESSAGE_ID) ||
    !PL_strcasecmp(name, HEADER_RESENT_MESSAGE_ID) ||
    !PL_strcasecmp(name, HEADER_REFERENCES))
    status = MimeHeaders_write_id_header_1(hdrs, name, c2, PR_TRUE, opt);
    else if (!PL_strcasecmp(name, HEADER_SUBJECT))
    status = MimeHeaders_write_subject_header_1(hdrs, name, c2, opt);    
    else
    status = MimeHeaders_write_random_header_1(hdrs, name, c2, opt, PR_FALSE);
    ****************************************/

    if (attachment)
      status = mimeEmitter->AddAttachmentField(name, c2);
    else
      status = mimeEmitter->AddHeaderField(name, c2);

    PR_Free(name);
    PR_Free(c2);
    
    if (status < 0) return status;
    if (!wrote_any_p) 
      wrote_any_p = (status > 0);
  }

  return 1;

// SHERRY NOT SURE FOR NOW-   return (wrote_any_p ? 1 : 0);
}

static int
MimeHeaders_write_microscopic_headers (MimeHeaders *hdrs,
									   MimeDisplayOptions *opt)
{
  int status = 1;
  char *subj  = MimeHeaders_get (hdrs, HEADER_SUBJECT, PR_FALSE, PR_FALSE);
  char *from  = MimeHeaders_get (hdrs, HEADER_FROM, PR_FALSE, PR_TRUE);
  char *date  = MimeHeaders_get (hdrs, HEADER_DATE, PR_FALSE, PR_TRUE);
  char *out;

  if (!from)
	from = MimeHeaders_get (hdrs, HEADER_SENDER, PR_FALSE, PR_TRUE);
  if (!date)
	date = MimeHeaders_get (hdrs, HEADER_RESENT_DATE, PR_FALSE, PR_TRUE);

  if (date && opt->reformat_date_fn)
	{
	  char *d2 = opt->reformat_date_fn(date, opt->stream_closure);
	  if (d2)
		{
		  PR_Free(date);
		  date = d2;
		}
	}


  /* Convert MIME-2 data.
   */
#define FROB(VAR) do {														\
  if (VAR)																	\
	{																		\
	  char *converted = 0;													\
	  PRInt32 converted_length = 0;											\
	  status = MimeHeaders_convert_rfc1522(opt, VAR, PL_strlen(VAR),		\
										   &converted, &converted_length);	\
	  if (status < 0) goto FAIL;											\
	  if (converted)														\
		{																	\
		  PR_ASSERT(converted_length == (PRInt32) PL_strlen(converted));		\
		  VAR = converted;													\
		}																	\
	} } while(0)
  FROB(from);
  FROB(subj);
  FROB(date);
#undef FROB

#define THLHMAAMS 900

  status = MimeHeaders_grow_obuffer (hdrs,
									 (subj ? PL_strlen(subj)*2 : 0) +
									 (from ? PL_strlen(from)   : 0) +
									 (date ? PL_strlen(date)   : 0) +
									 100
								 + THLHMAAMS
									 );
  if (status < 0) goto FAIL;

  if (subj) {
	PR_ASSERT(!hdrs->munged_subject);
	hdrs->munged_subject = PL_strdup(subj);
  }

  out = hdrs->obuffer;

  XP_SPRINTF(out, "\
<TR><TD VALIGN=TOP BGCOLOR=\"#CCCCCC\" ALIGN=RIGHT><B>%s: </B></TD>\
<TD VALIGN=TOP BGCOLOR=\"#CCCCCC\" width=100%%>\
<table border=0 cellspacing=0 cellpadding=0 width=100%%><tr>\
<td valign=top>", XP_GetString(MK_MIMEHTML_DISP_FROM));
  out += PL_strlen(out);

  if (from) {
	status = MimeHeaders_write(opt, hdrs->obuffer, PL_strlen(hdrs->obuffer));
	if (status < 0) goto FAIL;
	status = MimeHeaders_write_grouped_header_1(hdrs, NULL, from, opt, PR_TRUE);
	if (status < 0) goto FAIL;
	out = hdrs->obuffer;
  }

  PL_strcpy(out, "</td><td valign=top align=right nowrap>");
  out += PL_strlen(out);
  if (date) PL_strcpy(out, date);
  out += PL_strlen(out);
  PL_strcpy(out, "</td></tr></table></TD>");
  out += PL_strlen(out);

  /* ### This is where to insert something like <TD VALIGN=TOP align=right bgcolor=\"#CCCCCC\" ROWSPAN=2><IMG SRC=\"http://gooey/projects/dogbert/mail/M_both.gif\"></TD> if we want to do an image. */
  
  PL_strcpy(out,
			"</TR><TR><TD VALIGN=TOP BGCOLOR=\"#CCCCCC\" ALIGN=RIGHT><B>");
  out += PL_strlen(out);
  PL_strcpy(out, XP_GetString(MK_MIMEHTML_DISP_SUBJECT));
  out += PL_strlen(out);
  PL_strcpy(out, ": </B></TD><TD VALIGN=TOP BGCOLOR=\"#CCCCCC\">");
  out += PL_strlen(out);
  if (subj) {
	  status = NET_ScanForURLs(
							   NULL,
							   subj, PL_strlen(subj), out,
							   hdrs->obuffer_size - (out - hdrs->obuffer) - 10,
							   PR_TRUE);
	  if (status < 0) goto FAIL;
  } else {
	  PL_strcpy(out, "<BR>");
  }
  out += PL_strlen(out);
  PL_strcpy(out, "</TR>");
  out += PL_strlen(out);

  status = MimeHeaders_write(opt, hdrs->obuffer, PL_strlen(hdrs->obuffer));
  if (status < 0) goto FAIL;

  /* At this point, we've written out the one-line summary.
	 But we might want to write out some additional header lines, too...
   */


  /* If this was a redirected message, also show the Resent-From or
	 Resent-Sender field, to avoid nasty surprises.
   */
  status = MimeHeaders_write_address_header(hdrs, HEADER_RESENT_FROM, opt);
  if (status < 0) goto FAIL;
  if (status == 0)   /* meaning "nothing written" */
	status = MimeHeaders_write_address_header(hdrs, HEADER_RESENT_SENDER, opt);
  if (status < 0) goto FAIL;


  /* If this is the mail reader, show the full recipient list.
   */
  if (opt->headers == MimeHeadersMicroPlus)
	{
	  status = MimeHeaders_write_address_header (hdrs, HEADER_TO, opt);
	  if (status < 0) goto FAIL;
	  status = MimeHeaders_write_address_header (hdrs, HEADER_CC, opt);
	  if (status < 0) goto FAIL;
	  status = MimeHeaders_write_address_header (hdrs, HEADER_BCC, opt);
	  if (status < 0) goto FAIL;
	  status = MimeHeaders_write_news_header (hdrs, HEADER_NEWSGROUPS, opt);
	  if (status < 0) goto FAIL;
	}

 FAIL:

  PR_FREEIF(subj);
  PR_FREEIF(from);
  PR_FREEIF(date);

  return (status < 0 ? status : 1);
}


static int
MimeHeaders_write_citation_headers (MimeHeaders *hdrs, MimeDisplayOptions *opt)
{
  nsIPref *pref = GetPrefServiceManager(opt);   // Pref service manager
  int status;
  char *from = 0, *name = 0, *id = 0;
  const char *fmt = 0;
  char *converted = 0;
  PRInt32 converted_length = 0;

  PR_ASSERT(hdrs);
  if (!hdrs) return -1;

  if (!opt || !opt->output_fn)
	return 0;

  from = MimeHeaders_get(hdrs, HEADER_FROM, PR_FALSE, PR_FALSE);
  if (!from)
	from = MimeHeaders_get(hdrs, HEADER_SENDER, PR_FALSE, PR_FALSE);
  if (!from)
	from = PL_strdup("Unknown");
  if (!from)
	{
	  status = MK_OUT_OF_MEMORY;
	  goto FAIL;
	}

#if 0
  id = MimeHeaders_get(hdrs, HEADER_MESSAGE_ID, PR_FALSE, PR_FALSE);
#endif

  name = MSG_ExtractRFC822AddressNames (from);
  if (!name)
	{
	  name = from;
	  from = 0;
	}
  PR_FREEIF(from);

  fmt = (id
		 ? XP_GetString(MK_MSG_IN_MSG_X_USER_WROTE)
		 : XP_GetString(MK_MSG_USER_WROTE));

  status = MimeHeaders_grow_obuffer (hdrs,
									 PL_strlen(fmt) + PL_strlen(name) +
									 (id ? PL_strlen(id) : 0) + 58);
  if (status < 0) return status;

  if (opt->nice_html_only_p) {
	PRBool nReplyWithExtraLines = PR_FALSE, eReplyOnTop = PR_FALSE;
	if (pref)
  {
    pref->GetBoolPref("mailnews.reply_with_extra_lines", &nReplyWithExtraLines);
	  pref->GetBoolPref("mailnews.reply_on_top", &eReplyOnTop);
  }
	if (nReplyWithExtraLines && eReplyOnTop == 1) {
	  for (; nReplyWithExtraLines > 0; nReplyWithExtraLines--) {
		status = MimeHeaders_write(opt, "<BR>", 4);
		if (status < 0) return status;
	  }
	}
  }

  if (id)
	XP_SPRINTF(hdrs->obuffer, fmt, id, name);
  else
	XP_SPRINTF(hdrs->obuffer, fmt, name);

  status = MimeHeaders_convert_rfc1522(opt, hdrs->obuffer,
									   PL_strlen(hdrs->obuffer),
									   &converted, &converted_length);
  if (status < 0) return status;

  if (converted)
	{
	  status = MimeHeaders_write(opt, converted, converted_length);
	  if (status < 0) goto FAIL;
	}
  else
	{
	  status = MimeHeaders_write(opt, hdrs->obuffer, PL_strlen(hdrs->obuffer));
	  if (status < 0) goto FAIL;
	}

#define MHTML_BLOCKQUOTE_BEGIN   "<BLOCKQUOTE TYPE=CITE>"
  if (opt->nice_html_only_p) {
	  char ptr[] = MHTML_BLOCKQUOTE_BEGIN;
	  MimeHeaders_write(opt, ptr, PL_strlen(ptr));
  }

 FAIL:

  PR_FREEIF(from);
  PR_FREEIF(name);
  PR_FREEIF(id);
  return (status < 0 ? status : 1);
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
	  char nl[] = LINEBREAK;
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

int
MimeHeaders_write_headers_html (MimeHeaders *hdrs, MimeDisplayOptions *opt, PRBool attachment)
{
  int status = 0;
  PRBool wrote_any_p = PR_FALSE;

  PR_ASSERT(hdrs);
  if (!hdrs) 
    return -1;

  if (!opt || !opt->output_fn) 
    return 0;

  PR_FREEIF(hdrs->munged_subject);
  status = MimeHeaders_grow_obuffer (hdrs, /*210*/ 750);
  if (status < 0) return status;

  if (opt->fancy_headers_p) {
	  /* First, insert a table for the way-magic javascript appearing attachment
	     indicator.  The ending code for this is way below. */
#define MHTML_HEADER_TABLE       "<TABLE CELLPADDING=0 CELLSPACING=0 BORDER=0>"
#define MHTML_TABLE_BEGIN        "<TABLE><TR><TD>"

  	PL_strcpy(hdrs->obuffer, MHTML_TABLE_BEGIN MHTML_HEADER_TABLE);
  } 
  else
  	PL_strcpy (hdrs->obuffer, "<P>");

  status = MimeHeaders_write(opt, hdrs->obuffer, PL_strlen(hdrs->obuffer));
  if (status < 0) 
  {
    return status;
  }

  if ( (opt->headers == MimeHeadersAll) || (opt->headers == MimeHeadersOnly ) || attachment )
    status = MimeHeaders_write_all_headers (hdrs, opt, attachment);
  else if (opt->headers == MimeHeadersMicro ||
                          opt->headers == MimeHeadersMicroPlus)
    status = MimeHeaders_write_microscopic_headers (hdrs, opt);
  else if (opt->headers == MimeHeadersCitation)
    status = MimeHeaders_write_citation_headers (hdrs, opt);
  else
    status = MimeHeaders_write_interesting_headers (hdrs, opt);

  wrote_any_p = (status > 0);

  if (!wrote_any_p && opt->fancy_headers_p)
	{
#define MHTML_TABLE_COLUMN_BEGIN "<TR><TD><B><I>"
#define MHTML_TABLE_COLUMN_END   "</I></B></TD></TR>"

    const char *msg = XP_GetString(MK_MSG_NO_HEADERS);
	  PL_strcpy (hdrs->obuffer, MHTML_TABLE_COLUMN_BEGIN);
	  PL_strcat (hdrs->obuffer, msg);
	  PL_strcat (hdrs->obuffer, MHTML_TABLE_COLUMN_END);
	  status = MimeHeaders_write(opt, hdrs->obuffer, PL_strlen(hdrs->obuffer));
	  if (status < 0) goto FAIL;
	}

  if (opt->fancy_headers_p) {
#define MHTML_TABLE_END          "</TABLE>"

    PL_strcpy (hdrs->obuffer, MHTML_TABLE_END);
    
    /* OK, here's the ending code for the way magic javascript indicator. */
    if (!opt->nice_html_only_p && opt->fancy_links_p) {
      if (opt->attachment_icon_layer_id == 0) {
        static PRInt32 randomid = 1; /* Not very random. ### */
        char *mouseOverStatusString = XP_GetString(MK_MSG_SHOW_ATTACHMENT_PANE);
        opt->attachment_icon_layer_id = randomid;
        
#define MHTML_CLIP_START         "</TD><TD VALIGN=TOP><DIV "
#ifndef XP_MAC
#define MHTML_CLIP_DIM           "CLIP=0,0,30,30 "
#else
#define MHTML_CLIP_DIM           "CLIP=0,0,48,30 "
#endif
#define MHTML_NOATTACH_ID        "ID=noattach%ld>"
#define MHTML_ENDBEGIN_DIV       "</DIV><DIV LOCKED "
#define MHTML_ATTACH_ID          "ID=attach%ld style=\"display: 'none';\" >"
#define MHTML_DISP_ICON_LINK     "<a href=javascript:toggleAttachments(); onMouseOver=\"window.status='%s'; return true\"; onMouseOut=\"window.status=''; return true\" PRIVATE>"
#define MHTML_ATTACH_ICON        "<IMG SRC=resource:/res/network/gopher-text.gif BORDER=0>"
#define MHTML_CLIP_END           "</a></DIV>"

        /* Make sure we have enough buffer space. Grow it if needed. */
        MimeHeaders_grow_obuffer(hdrs, 
									 PL_strlen(hdrs->obuffer)+
                   PL_strlen(mouseOverStatusString)+
                   64 +    /* 64 counts for two long number*/
                   412);   /* 412 counts for the following constant 
                              string. It is less than 412  */
        XP_SPRINTF(hdrs->obuffer + PL_strlen(hdrs->obuffer),
          /* We set a CLIP=0,0,30,30 so that the attachment icon's layer cannot*/
          /* be exploited for header spoofing.  This means that if the attachment */
          /* icon changes size, then we have to change this size too */
          MHTML_CLIP_START
          MHTML_CLIP_DIM
          MHTML_NOATTACH_ID
          MHTML_ENDBEGIN_DIV
          MHTML_CLIP_DIM
          MHTML_ATTACH_ID
          MHTML_DISP_ICON_LINK 
          MHTML_ATTACH_ICON
          MHTML_CLIP_END,
          (long) randomid, (long) randomid, mouseOverStatusString);

        randomid++;
      }
    }
#define MHTML_COL_AND_TABLE_END  "</td></tr></table>"

    PL_strcat(hdrs->obuffer, MHTML_COL_AND_TABLE_END);
  } 
  else
    PL_strcpy (hdrs->obuffer, "<P>");
  
  status = MimeHeaders_write(opt, hdrs->obuffer, PL_strlen(hdrs->obuffer));
  if (status < 0) goto FAIL;
  if (hdrs->munged_subject) {
    char* t2 = NET_EscapeHTML(hdrs->munged_subject);
    PR_FREEIF(hdrs->munged_subject);
    if (t2) {
      status = MimeHeaders_grow_obuffer(hdrs, PL_strlen(t2) + 20);
      if (status >= 0) {
#define MHTML_TITLE              "<TITLE>%s</TITLE>\n"

        XP_SPRINTF(hdrs->obuffer, MHTML_TITLE, t2);
        status = MimeHeaders_write(opt, hdrs->obuffer,
          PL_strlen(hdrs->obuffer));
      }
    }
    PR_FREEIF(t2);
    if (status < 0) goto FAIL;
  }
  
 FAIL:
  MimeHeaders_compact (hdrs);
  return status;
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
  int status = 0;

  nsIMimeEmitter   *mimeEmitter = GetMimeEmitter(opt);
  mimeEmitter->StartAttachment(lname, content_type, lname_url);

  status = MimeHeaders_write_all_headers (hdrs, opt, TRUE);
  mimeEmitter->AddAttachmentField(HEADER_X_MOZILLA_PART_URL, lname_url);

  mimeEmitter->EndAttachment();

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
			while((*p2 == CR) || (*p2 == LF) || XP_IS_SPACE(*p2));

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
		if (cvt && cvt != returnVal)
			returnVal = cvt;
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
