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

/* mimehdrs.c --- MIME header parser, version 2 (see mimei.h).
   Created: Jamie Zawinski <jwz@netscape.com>, 15-May-96.
 */


#include "mimei.h"
#include "xpgetstr.h"
#include "libi18n.h"

#ifndef MOZILLA_30
# include "msgcom.h"
#include "prefapi.h"
#endif /* !MOZILLA_30 */

extern int MK_OUT_OF_MEMORY;
extern int MK_MSG_NO_HEADERS;
extern int MK_MSG_MIME_MAC_FILE;
extern int MK_MSG_IN_MSG_X_USER_WROTE;
extern int MK_MSG_USER_WROTE;
extern int MK_MSG_UNSPECIFIED_TYPE;
extern int MK_MSG_XSENDER_INTERNAL;
extern int MK_MSG_ADDBOOK_MOUSEOVER_TEXT;

extern int MK_MIMEHTML_DISP_SUBJECT;
extern int MK_MIMEHTML_DISP_RESENT_COMMENTS;
extern int MK_MIMEHTML_DISP_RESENT_DATE;
extern int MK_MIMEHTML_DISP_RESENT_SENDER;
extern int MK_MIMEHTML_DISP_RESENT_FROM;
extern int MK_MIMEHTML_DISP_RESENT_TO;
extern int MK_MIMEHTML_DISP_RESENT_CC;
extern int MK_MIMEHTML_DISP_DATE;
extern int MK_MIMEHTML_DISP_SUBJECT;
extern int MK_MIMEHTML_DISP_SENDER;
extern int MK_MIMEHTML_DISP_FROM;
extern int MK_MIMEHTML_DISP_REPLY_TO;
extern int MK_MIMEHTML_DISP_ORGANIZATION;
extern int MK_MIMEHTML_DISP_TO;
extern int MK_MIMEHTML_DISP_CC;
extern int MK_MIMEHTML_DISP_NEWSGROUPS;
extern int MK_MIMEHTML_DISP_FOLLOWUP_TO;
extern int MK_MIMEHTML_DISP_REFERENCES;
extern int MK_MIMEHTML_DISP_MESSAGE_ID;
extern int MK_MIMEHTML_DISP_RESENT_MESSAGE_ID;
extern int MK_MIMEHTML_DISP_BCC;
extern int MK_MIMEHTML_SHOW_SECURITY_ADVISOR;

extern int MK_MIMEHTML_ENC_AND_SIGNED;
extern int MK_MIMEHTML_SIGNED;
extern int MK_MIMEHTML_ENCRYPTED;
extern int MK_MIMEHTML_CERTIFICATES;
extern int MK_MIMEHTML_ENC_SIGNED_BAD;
extern int MK_MIMEHTML_SIGNED_BAD;
extern int MK_MIMEHTML_ENCRYPTED_BAD;
extern int MK_MIMEHTML_CERTIFICATES_BAD;





MimeHeaders *
MimeHeaders_new (void)
{
  MimeHeaders *hdrs = (MimeHeaders *) XP_ALLOC(sizeof(MimeHeaders));
  if (!hdrs) return 0;

  XP_MEMSET(hdrs, 0, sizeof(*hdrs));
  hdrs->done_p = FALSE;

  return hdrs;
}

void
MimeHeaders_free (MimeHeaders *hdrs)
{
  if (!hdrs) return;
  FREEIF(hdrs->all_headers);
  FREEIF(hdrs->heads);
  FREEIF(hdrs->obuffer);
  FREEIF(hdrs->munged_subject);
  hdrs->obuffer_fp = 0;
  hdrs->obuffer_size = 0;

# ifdef DEBUG__
  {
	int i, size = sizeof(*hdrs);
	uint32 *array = (uint32*) hdrs;
	for (i = 0; i < (size / sizeof(*array)); i++)
	  array[i] = (uint32) 0xDEADBEEF;
  }
# endif /* DEBUG */

  XP_FREE(hdrs);
}


MimeHeaders *
MimeHeaders_copy (MimeHeaders *hdrs)
{
  MimeHeaders *hdrs2;
  if (!hdrs) return 0;

  hdrs2 = (MimeHeaders *) XP_ALLOC(sizeof(*hdrs));
  if (!hdrs2) return 0;
  XP_MEMSET(hdrs2, 0, sizeof(*hdrs2));

  if (hdrs->all_headers)
	{
	  hdrs2->all_headers = (char *) XP_ALLOC(hdrs->all_headers_fp);
	  if (!hdrs2->all_headers)
		{
		  XP_FREE(hdrs2);
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
	  hdrs2->heads = (char **) XP_ALLOC(hdrs->heads_size
										* sizeof(*hdrs->heads));
	  if (!hdrs2->heads)
		{
		  FREEIF(hdrs2->all_headers);
		  XP_FREE(hdrs2);
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
  XP_ASSERT(hdrs);
  if (!hdrs) return;

  FREEIF(hdrs->obuffer);
  hdrs->obuffer_fp = 0;
  hdrs->obuffer_size = 0;

  /* These really shouldn't have gotten out of whack again. */
  XP_ASSERT(hdrs->all_headers_fp <= hdrs->all_headers_size &&
			hdrs->all_headers_fp + 100 > hdrs->all_headers_size);
}


static int MimeHeaders_build_heads_list(MimeHeaders *hdrs);

int
MimeHeaders_parse_line (const char *buffer, int32 size, MimeHeaders *hdrs)
{
  int status = 0;
  int desired_size;

  XP_ASSERT(hdrs);
  if (!hdrs) return -1;

  /* Don't try and feed me more data after having fed me a blank line... */
  XP_ASSERT(!hdrs->done_p);
  if (hdrs->done_p) return -1;

  if (!buffer || size == 0 || *buffer == CR || *buffer == LF)
	{
	  /* If this is a blank line, we're done.
	   */
	  hdrs->done_p = TRUE;
	  return MimeHeaders_build_heads_list(hdrs);
	}

  /* Tack this data on to the end of our copy.
   */
  desired_size = hdrs->all_headers_fp + size + 1;
  if (desired_size >= hdrs->all_headers_size)
	{
	  status = msg_GrowBuffer (desired_size, sizeof(char), 255,
							   &hdrs->all_headers, &hdrs->all_headers_size);
	  if (status < 0) return status;
	}
  XP_MEMCPY(hdrs->all_headers+hdrs->all_headers_fp, buffer, size);
  hdrs->all_headers_fp += size;

  return 0;
}

static int
MimeHeaders_build_heads_list(MimeHeaders *hdrs)
{
  char *s;
  char *end;
  int i;
  XP_ASSERT(hdrs);
  if (!hdrs) return -1;

  XP_ASSERT(hdrs->done_p && !hdrs->heads);
  if (!hdrs->done_p || hdrs->heads)
	return -1;

  if (hdrs->all_headers_fp == 0)
	{
	  /* Must not have been any headers (we got the blank line right away.) */
	  FREEIF (hdrs->all_headers);
	  hdrs->all_headers_size = 0;
	  return 0;
	}

  /* At this point, we might as well realloc all_headers back down to the
	 minimum size it must be (it could be up to 1k bigger.)  But don't
	 bother if we're only off by a tiny bit. */
  XP_ASSERT(hdrs->all_headers_fp <= hdrs->all_headers_size);
  if (hdrs->all_headers_fp + 60 <= hdrs->all_headers_size)
	{
	  char *s = XP_REALLOC(hdrs->all_headers, hdrs->all_headers_fp);
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
  hdrs->heads = (char **) XP_ALLOC((hdrs->heads_size + 1) * sizeof(char *));
  if (!hdrs->heads)
	return MK_OUT_OF_MEMORY;
  XP_MEMSET(hdrs->heads, 0, (hdrs->heads_size + 1) * sizeof(char *));


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
		  XP_ASSERT(! (i > hdrs->heads_size));
		  if (i > hdrs->heads_size) return -1;
		  hdrs->heads[i++] = s;
		}
	}

  return 0;
}


char *
MimeHeaders_get (MimeHeaders *hdrs, const char *header_name,
				 XP_Bool strip_p, XP_Bool all_p)
{
  int i;
  int name_length;
  char *result = 0;

  XP_ASSERT(hdrs);
  if (!hdrs) return 0;
  XP_ASSERT(header_name);
  if (!header_name) return 0;

  /* Specifying strip_p and all_p at the same time doesn't make sense... */
  XP_ASSERT(!(strip_p && all_p));

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
	  hdrs->done_p = TRUE;
	  status = MimeHeaders_build_heads_list(hdrs);
	  if (status < 0) return 0;
	}

  if (!hdrs->heads)	  /* Must not have been any headers. */
	{
	  XP_ASSERT(hdrs->all_headers_fp == 0);
	  return 0;
	}

  name_length = XP_STRLEN(header_name);

  for (i = 0; i < hdrs->heads_size; i++)
	{
	  char *head = hdrs->heads[i];
	  char *end = (i == hdrs->heads_size-1
				   ? hdrs->all_headers + hdrs->all_headers_fp
				   : hdrs->heads[i+1]);
	  char *colon, *ocolon;

	  XP_ASSERT(head);
	  if (!head) continue;

	  /* Quick hack to skip over BSD Mailbox delimiter. */
	  if (i == 0 && head[0] == 'F' && !XP_STRNCMP(head, "From ", 5))
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
	  if (strncasecomp(header_name, head, name_length))
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
			result = (char *) XP_ALLOC(end - contents + 1);
			if (!result)
			  return 0;
			s = result;
		  }
		else
		  {
			int32 L = XP_STRLEN(result);
			s = (char *) XP_REALLOC(result, (L + (end - contents + 10)));
			if (!s)
			  {
				XP_FREE(result);
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

		/* Now copy the header's contents in...
		 */
		XP_MEMCPY(s, contents, end - contents);
		s[end - contents] = 0;

		/* If we only wanted the first occurence of this header, we're done. */
		if (!all_p) break;
	  }
	}

  if (result && !*result)  /* empty string */
	{
	  XP_FREE(result);
	  return 0;
	}

  return result;
}

char *
MimeHeaders_get_parameter (const char *header_value, const char *parm_name)
{
  const char *str;
  int32 parm_len;
  if (!header_value || !parm_name || !*header_value || !*parm_name)
	return 0;

  /* The format of these header lines is
	 <token> [ ';' <token> '=' <token-or-quoted-string> ]*
   */

  str = header_value;
  parm_len = XP_STRLEN(parm_name);

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

	  XP_ASSERT(!XP_IS_SPACE(*str)); /* should be after whitespace already */

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
		  !strncasecomp(token_start, parm_name, parm_len))
		{
		  char *s = (char *) XP_ALLOC ((value_end - value_start) + 1);
		  if (! s) return 0;  /* MK_OUT_OF_MEMORY */
		  XP_MEMCPY (s, value_start, value_end - value_start);
		  s [value_end - value_start] = 0;
		  return s;
		}

	  /* str now points after the end of the value.
		 skip over whitespace, ';', whitespace. */
	  while (XP_IS_SPACE (*str)) str++;
	  if (*str == ';') str++;
	  while (XP_IS_SPACE (*str)) str++;
	}
  return 0;
}


/* Emitting HTML
 */

#define MIME_HEADER_TABLE "<TABLE CELLPADDING=0 CELLSPACING=0 BORDER=0>"
#define HEADER_START_JUNK  "<TR><TH VALIGN=BASELINE ALIGN=RIGHT NOWRAP>"
#define HEADER_MIDDLE_JUNK ": </TH><TD>"
#define HEADER_END_JUNK    "</TD></TR>"


#define MimeHeaders_write(OPT,DATA,LENGTH) \
	MimeOptions_write((OPT), (DATA), (LENGTH), TRUE);


static char *
MimeHeaders_default_news_link_generator (const char *dest, void *closure,
										 MimeHeaders *headers)
{
  /* This works as both the generate_news_url_fn and as the
	 generate_reference_url_fn. */
  char *prefix = "news:";
  char *new_dest = NET_Escape (dest, URL_XALPHAS);
  char *result = (char *) XP_ALLOC (XP_STRLEN (new_dest) +
									XP_STRLEN (prefix) + 1);
  if (result)
	{
	  XP_STRCPY (result, prefix);
	  XP_STRCAT (result, new_dest);
	}
  FREEIF (new_dest);
  return result;
}


#ifdef MOZILLA_30
static char *
MimeHeaders_default_mailto_link_generator (const char *dest, void *closure,
										   MimeHeaders *headers)
{
  /* For now, don't emit mailto links over email addresses. */
  return 0;
}

#else  /* !MOZILLA_30 */

static char *mime_escape_quotes (char *src)
{
	/* Make sure any single or double quote is escaped with a backslash */
	char *dst = XP_ALLOC((2 * XP_STRLEN(src)) + 1);
	if (dst)
	{
		char *walkDst = dst;
		while (*src)
		{
			if (*src == '\'' || *src == '\"' ) /* is it a quote? */
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
  char* result = NULL;
  char* tmp;
  char* tmp2;
  char* mouseOverText = NULL;
  int j;
  int num = MSG_ParseRFC822Addresses(dest, &names, &addresses);
  XP_ASSERT(num >= 1);
  for (j=0 ; j<num ; j++) {
	if (addr) {
	  addr = addr + XP_STRLEN(addr) + 1;
	  name = name + XP_STRLEN(name) + 1;
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

	tmp = PR_smprintf("begin:vcard\nfn:%s\nemail;internet:%s\nend:vcard\n",
					  (unquotedName && *unquotedName) ? unquotedName : unquotedAddr,
					  unquotedAddr);

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
				XP_FREE(localizedPart);
				*((char**)closure) = mouseOverText;
			}
			XP_FREE(jsSafeName);
		}
	}

	FREEIF(unquotedAddr);
	FREEIF(unquotedName);

	if (!tmp) break;
	tmp2 = NET_Escape(tmp, URL_XALPHAS);
	XP_FREE(tmp);
	if (!tmp2) break;
	result = PR_smprintf("addbook:add?vcard=%s", tmp2);
	XP_FREE(tmp2);
	break;
  }
  FREEIF(names);
  FREEIF(addresses);
  return result;
}

#endif /* !MOZILLA_30 */


#define MimeHeaders_grow_obuffer(hdrs, desired_size) \
  ((((long) (desired_size)) >= ((long) (hdrs)->obuffer_size)) ? \
   msg_GrowBuffer ((desired_size), sizeof(char), 255, \
				   &(hdrs)->obuffer, &(hdrs)->obuffer_size) \
   : 0)


static int
MimeHeaders_convert_rfc1522(MimeDisplayOptions *opt,
							const char *input, int32 input_length,
							char **output_ret, int32 *output_length_ret)
{
  *output_ret = 0;
  *output_length_ret = 0;
  if (input && *input && opt && opt->rfc1522_conversion_fn)
	{
	  char *converted = 0;
	  int32 converted_len = 0;
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
		  FREEIF(converted);
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
  int16 output_csid = 0;
  int display_key =
   (!strcasecomp(name,HEADER_BCC)			 ? MK_MIMEHTML_DISP_BCC :
	!strcasecomp(name,HEADER_CC)			 ? MK_MIMEHTML_DISP_CC :
	!strcasecomp(name,HEADER_DATE)			 ? MK_MIMEHTML_DISP_DATE :
	!strcasecomp(name,HEADER_FOLLOWUP_TO)	 ? MK_MIMEHTML_DISP_FOLLOWUP_TO :
	!strcasecomp(name,HEADER_FROM)			 ? MK_MIMEHTML_DISP_FROM :
	!strcasecomp(name,HEADER_MESSAGE_ID)	 ? MK_MIMEHTML_DISP_MESSAGE_ID :
	!strcasecomp(name,HEADER_NEWSGROUPS)	 ? MK_MIMEHTML_DISP_NEWSGROUPS :
	!strcasecomp(name,HEADER_ORGANIZATION)	 ? MK_MIMEHTML_DISP_ORGANIZATION :
	!strcasecomp(name,HEADER_REFERENCES)	 ? MK_MIMEHTML_DISP_REFERENCES :
	!strcasecomp(name,HEADER_REPLY_TO)		 ? MK_MIMEHTML_DISP_REPLY_TO :
	!strcasecomp(name,HEADER_RESENT_CC)		 ? MK_MIMEHTML_DISP_RESENT_CC :
	!strcasecomp(name,HEADER_RESENT_COMMENTS)
										   ? MK_MIMEHTML_DISP_RESENT_COMMENTS :
	!strcasecomp(name,HEADER_RESENT_DATE)	 ? MK_MIMEHTML_DISP_RESENT_DATE :
	!strcasecomp(name,HEADER_RESENT_FROM)	 ? MK_MIMEHTML_DISP_RESENT_FROM :
	!strcasecomp(name,HEADER_RESENT_MESSAGE_ID)
										 ? MK_MIMEHTML_DISP_RESENT_MESSAGE_ID :
	!strcasecomp(name,HEADER_RESENT_SENDER)  ? MK_MIMEHTML_DISP_RESENT_SENDER :
	!strcasecomp(name,HEADER_RESENT_TO)		 ? MK_MIMEHTML_DISP_RESENT_TO :
	!strcasecomp(name,HEADER_SENDER)		 ? MK_MIMEHTML_DISP_SENDER :
	!strcasecomp(name,HEADER_SUBJECT)		 ? MK_MIMEHTML_DISP_SUBJECT :
	!strcasecomp(name,HEADER_TO)			 ? MK_MIMEHTML_DISP_TO :
    !strcasecomp(name,HEADER_SUBJECT)		 ? MK_MIMEHTML_DISP_SUBJECT :
	0);
  
  if (!display_key)
	return name;

#ifndef MOZILLA_30
  output_csid = INTL_CharSetNameToID((opt->override_charset
									  ? opt->override_charset
									  : opt->default_charset));
  new_name = XP_GetStringForHTML(display_key, output_csid, (char*)name);
#endif /* !MOZILLA_30 */

  if (new_name)
	return new_name;
  else
	return name;
}


static int
MimeHeaders_write_random_header_1 (MimeHeaders *hdrs,
								   const char *name, const char *contents,
								   MimeDisplayOptions *opt,
								   XP_Bool subject_p)
{
  int status = 0;
  int32 contents_length;
  char *out;
  char *converted = 0;
  int32 converted_length = 0;

  if (!contents && subject_p)
	contents = "";
  else if (!contents)
	return 0;

  name = MimeHeaders_localize_header_name(name, opt);

  contents_length = XP_STRLEN(contents);
  status = MimeHeaders_convert_rfc1522(opt, contents, contents_length,
									   &converted, &converted_length);
  if (status < 0) return status;
  if (converted)
	{
	  contents = converted;
	  contents_length = converted_length;
	}

  status = MimeHeaders_grow_obuffer (hdrs, XP_STRLEN (name) + 100 +
									 (contents_length * 4));
  if (status < 0)
	{
	  FREEIF(converted);
	  return status;
	}

  out = hdrs->obuffer;

  if (subject_p && contents)
	{
	  XP_ASSERT(hdrs->munged_subject == NULL);
	  hdrs->munged_subject = XP_STRDUP(contents);
	}

  if (opt->fancy_headers_p)
	{
	  XP_STRCPY (out, HEADER_START_JUNK); out += XP_STRLEN (out);
	  XP_STRCPY (out, name); out += XP_STRLEN (out);
	  XP_STRCPY (out, HEADER_MIDDLE_JUNK); out += XP_STRLEN (out);
	  if (subject_p)
		{
		  XP_STRCPY (out, "<B>"); out += XP_STRLEN (out);
		}

	  /* Copy `contents' to `out', quoting HTML along the way.
		 Note: this function does no charset conversion; that has
		 already been done.
	   */
	  status = NET_ScanForURLs (
#ifndef MOZILLA_30
								NULL,
#endif /* !MOZILLA_30 */
								contents, contents_length, out,
								hdrs->obuffer_size - (out - hdrs->obuffer) -10,
								TRUE);
	  if (status < 0) return status;
	  out += XP_STRLEN(out);
	  XP_ASSERT(out < (hdrs->obuffer + hdrs->obuffer_size));

	  XP_STRCPY (out, HEADER_END_JUNK); out += XP_STRLEN (out);
	}
  else
	{
	  /* The non-fancy version (no tables): for converting to plain text. */
	  char *s = NET_EscapeHTML (contents);
	  if (s)
		{
		  char *start, *end, *data_end;
		  XP_STRCPY (out, "<NOBR><B>"); out += XP_STRLEN (out);
		  XP_STRCPY (out, name); out += XP_STRLEN (out);
		  XP_STRCPY (out, ": </B>"); out += XP_STRLEN (out);

		  data_end = s + XP_STRLEN (s);
		  start = s;
		  end = start;
		  while (end < data_end)
			for (end = start; end < data_end; end++)
			  if (*end == CR || *end == LF)
				{
				  XP_MEMCPY (out, start, end - start); out += (end - start);
				  XP_STRCPY (out, "<BR>&nbsp;&nbsp;&nbsp;&nbsp;");
				  out += XP_STRLEN (out);
				  if (*end == CR && end < data_end && end[1] == LF)
					end++;
				  start = end + 1;
				}
		  if (start < end)
			{
			  XP_MEMCPY (out, start, end - start); out += (end - start);
			}
		  XP_STRCPY (out, "</NOBR><BR>"); out += XP_STRLEN (out);
		  XP_FREE (s);
		}
	}
  *out = 0; 

  status = MimeHeaders_write(opt, hdrs->obuffer, out - hdrs->obuffer);
  FREEIF(converted);
  if (status < 0)
	return status;
  else
	return 1;
}


static int
MimeHeaders_write_random_header (MimeHeaders *hdrs, const char *name,
								 XP_Bool all_p, MimeDisplayOptions *opt)
{
  int status = 0;
  char *contents = MimeHeaders_get (hdrs, name, FALSE, all_p);
  if (!contents) return 0;
  status = MimeHeaders_write_random_header_1(hdrs, name, contents, opt, FALSE);
  XP_FREE(contents);
  return status;
}


static int
MimeHeaders_write_subject_header_1 (MimeHeaders *hdrs, const char *name,
									const char *contents,
									MimeDisplayOptions *opt)
{
  return MimeHeaders_write_random_header_1(hdrs, name, contents, opt, TRUE);
}

static int
MimeHeaders_write_subject_header (MimeHeaders *hdrs, const char *name,
								  MimeDisplayOptions *opt)
{
  int status = 0;
  char *contents = MimeHeaders_get (hdrs, name, FALSE, FALSE);
  status = MimeHeaders_write_subject_header_1 (hdrs, name, contents, opt);
  FREEIF(contents);
  return status;
}


static int
MimeHeaders_write_grouped_header_1 (MimeHeaders *hdrs, const char *name,
									const char *contents,
									MimeDisplayOptions *opt,
									XP_Bool mail_header_p)
{
  int status = 0;
  int32 contents_length;
  char *converted = 0;
  char *out;
  if (!contents)
	return 0;

  if (!opt->fancy_headers_p)
	return MimeHeaders_write_random_header (hdrs, name, TRUE, opt);

  if (name) name = MimeHeaders_localize_header_name(name, opt);

  contents_length = XP_STRLEN(contents);

  if (mail_header_p)
	{
	  int32 converted_length = 0;
	  status = MimeHeaders_convert_rfc1522(opt, contents, contents_length,
										   &converted, &converted_length);
	  if (status < 0) return status;
	  if (converted)
		{
		  contents = converted;
		  contents_length = converted_length;
		}
	}

#ifndef MOZILLA_30
  /* grow obuffer with potential XSENDER AUTH string */
  status = MimeHeaders_grow_obuffer (hdrs, (name ? XP_STRLEN (name) : 0)
									 + 100 + 2 *
									 XP_STRLEN(XP_GetString(MK_MSG_XSENDER_INTERNAL)) +
									 (contents_length * 4));
#else
  status = MimeHeaders_grow_obuffer (hdrs, (name ? XP_STRLEN (name) : 0)
									 + 100 +
									 (contents_length * 4));
#endif
  if (status < 0) goto FAIL;

  out = hdrs->obuffer;
  if (name) {
	  XP_STRCPY (out, HEADER_START_JUNK); out += XP_STRLEN (out);
	  XP_STRCPY (out, name); out += XP_STRLEN (out);
	  XP_STRCPY (out, HEADER_MIDDLE_JUNK); out += XP_STRLEN (out);
  }

  status = MimeHeaders_write(opt, hdrs->obuffer, out - hdrs->obuffer);
  if (status < 0) goto FAIL;

  {
	const char *data_end = contents + contents_length;
	const char *last_end = 0;
	const char *this_start = contents;
	const char *this_end = this_start;
	while (this_end < data_end)
	  {
		int32 paren_depth = 0;

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
			XP_STRCPY (hdrs->obuffer, "</NOBR>");
			status = MimeHeaders_write(opt, hdrs->obuffer,
									   XP_STRLEN(hdrs->obuffer));
			if (status < 0) goto FAIL;

			XP_MEMCPY (hdrs->obuffer, last_end, this_start - last_end);
			hdrs->obuffer [this_start - last_end] = 0;
			s = NET_EscapeHTML (hdrs->obuffer);
			if (!s)
			  {
				status = MK_OUT_OF_MEMORY;
				goto FAIL;
			  }
			status = MimeHeaders_write(opt, s, XP_STRLEN (s));
			XP_FREE (s);
			if (status < 0) goto FAIL;

			/* Emit a space, to make sure long newsgroup lines, or any
			   header line with no spaces after the commas, wrap. */
			hdrs->obuffer[0] = ' ';
			hdrs->obuffer[1] = 0;
			status = MimeHeaders_write(opt, hdrs->obuffer,
									   XP_STRLEN (hdrs->obuffer));
			if (status < 0) goto FAIL;
		  }

		/* Push out this address-or-newsgroup. */
		if (this_start != this_end)
		  {
			char *s;
			MimeHTMLGeneratorFunction fn = 0;
			void *arg;
			char *link, *link2;
			XP_Bool mail_p;
			char *extraAnchorText = NULL;

			XP_MEMCPY (hdrs->obuffer, this_start, this_end - this_start);
			hdrs->obuffer [this_end - this_start] = 0;
			s = NET_EscapeHTML (hdrs->obuffer);
			if (!s)
			  {
				status = MK_OUT_OF_MEMORY;
				goto FAIL;
			  }
			mail_p = (mail_header_p || !strcasecomp ("poster", hdrs->obuffer));

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
#ifdef MOZILLA_30
					fn = MimeHeaders_default_mailto_link_generator;
					arg = 0;
#else  /* !MOZILLA_30 */
					fn = MimeHeaders_default_addbook_link_generator;
					arg = NULL; /* ###phil fix this &extraAnchorText; */
#endif /* !MOZILLA_30 */
				  }
				else
				  {
					fn = MimeHeaders_default_news_link_generator;
					arg = 0;
				  }
			  }

			/* If this is the "From" header, or if this is a
			   "Followup-To: poster" entry, let the Reply-To address win.
			 */
			if (mail_header_p
				? (name && !strcasecomp(name, HEADER_FROM))
				: mail_p)
			  {
				char *r = 0;
				r         = MimeHeaders_get (hdrs, HEADER_REPLY_TO,
											 FALSE, TRUE);
				if (!r) r = MimeHeaders_get (hdrs, HEADER_FROM, FALSE, TRUE);
				if (!r) r = MimeHeaders_get (hdrs, HEADER_SENDER, FALSE, TRUE);

				if (r)
				  {
					XP_STRCPY (hdrs->obuffer, r);
					XP_FREE(r);
				  }
				else
				  *hdrs->obuffer = 0;
			  }

			if (*hdrs->obuffer && opt->fancy_links_p)
			  link = fn (hdrs->obuffer, arg, hdrs);
			else
			  link = 0;

			link2 = (link ? NET_EscapeHTML (link) : 0);
			FREEIF (link);
			link = link2;

			if (link)
			  {
				status = MimeHeaders_grow_obuffer (hdrs, XP_STRLEN (link) +
												   XP_STRLEN (s) + 100);
				if (status < 0) goto FAIL;
			  }
			out = hdrs->obuffer;
			if (link)
			  {
				XP_STRCPY (out, "<A HREF=\""); out += XP_STRLEN (out);
				XP_STRCPY (out, link); out += XP_STRLEN (out);
				XP_STRCPY (out, "\""); out += 1;
				if (extraAnchorText)
				{
					XP_STRCPY (out, extraAnchorText);
					out += XP_STRLEN(out);
				}
				XP_STRCPY (out, ">"); out += 1;
			  }
			XP_STRCPY (out, "<NOBR>"); out += XP_STRLEN (out);

			XP_STRCPY (out, s); out += XP_STRLEN (out);

			XP_FREE (s);
			if (link)
			  {
				XP_STRCPY (out, "</A>"); out += XP_STRLEN (out);
				XP_FREE (link);
			  }
			if (extraAnchorText)
				XP_FREE (extraAnchorText);
#ifndef MOZILLA_30
			/* Begin hack of out of envelope XSENDER info */
			if (name && !strcasecomp(name, HEADER_FROM)) { 
				char * statusLine = MimeHeaders_get (hdrs, HEADER_X_MOZILLA_STATUS, FALSE, FALSE);
				uint16 flags =0;

				#define UNHEX(C) \
								((C >= '0' && C <= '9') ? C - '0' : \
								 ((C >= 'A' && C <= 'F') ? C - 'A' + 10 : \
								  ((C >= 'a' && C <= 'f') ? C - 'a' + 10 : 0)))

				if (statusLine && XP_STRLEN(statusLine) == 4) {
					int i;
					char *cp = statusLine;
					for (i=0; i < 4; i++, cp++) {
						flags = (flags << 4) | UNHEX(*cp);
					}
					FREEIF(statusLine);
				}

				if (flags & MSG_FLAG_SENDER_AUTHED) {
					XP_STRCPY (out, XP_GetString(MK_MSG_XSENDER_INTERNAL));
					out += XP_STRLEN (out);
				}
			}
			/* End of XSENDER info */
#endif  /* MOZILLA_30 */
			status = MimeHeaders_write(opt, hdrs->obuffer,
									   XP_STRLEN (hdrs->obuffer));
			if (status < 0) goto FAIL;
		  }

		/* now go around again */
		last_end = this_end;
		this_start = last_end;
	  }
  }

  out = hdrs->obuffer;
  XP_STRCPY (out, "</NOBR>"); out += XP_STRLEN (out);
  if (name) XP_STRCPY (out, HEADER_END_JUNK); out += XP_STRLEN (out);
  *out = 0;
  status = MimeHeaders_write(opt, hdrs->obuffer, out - hdrs->obuffer);
  if (status < 0) goto FAIL;
  else status = 1;

 FAIL:
  FREEIF(converted);
  return status;
}

static int
MimeHeaders_write_grouped_header (MimeHeaders *hdrs, const char *name,
								  MimeDisplayOptions *opt,
								  XP_Bool mail_header_p)
{
  int status = 0;
  char *contents = MimeHeaders_get (hdrs, name, FALSE, TRUE);
  if (!contents) return 0;
  status = MimeHeaders_write_grouped_header_1 (hdrs, name, contents, opt,
											   mail_header_p);
  XP_FREE(contents);
  return status;
}

#define MimeHeaders_write_address_header(hdrs,name,opt) \
	MimeHeaders_write_grouped_header(hdrs,name,opt,TRUE)
#define MimeHeaders_write_news_header(hdrs,name,opt) \
	MimeHeaders_write_grouped_header(hdrs,name,opt,FALSE)

#define MimeHeaders_write_address_header_1(hdrs,name,contents,opt) \
	MimeHeaders_write_grouped_header_1(hdrs,name,contents,opt,TRUE)
#define MimeHeaders_write_news_header_1(hdrs,name,contents,opt) \
	MimeHeaders_write_grouped_header_1(hdrs,name,contents,opt,FALSE)


static int
MimeHeaders_write_id_header_1 (MimeHeaders *hdrs, const char *name,
							   const char *contents, XP_Bool show_ids,
							   MimeDisplayOptions *opt)
{
  int status = 0;
  int32 contents_length;
  char *out;
  if (!contents)
	return 0;

  if (!opt->fancy_headers_p)
	return MimeHeaders_write_random_header (hdrs, name, TRUE, opt);

  name = MimeHeaders_localize_header_name(name, opt);

  contents_length = XP_STRLEN(contents);
  status = MimeHeaders_grow_obuffer (hdrs, XP_STRLEN (name) + 100 +
									 (contents_length * 4));
  if (status < 0) goto FAIL;

  out = hdrs->obuffer;
  XP_STRCPY (out, HEADER_START_JUNK); out += XP_STRLEN (out);
  XP_STRCPY (out, name); out += XP_STRLEN (out);
  XP_STRCPY (out, HEADER_MIDDLE_JUNK); out += XP_STRLEN (out);

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
			status = MimeHeaders_write(opt, s, XP_STRLEN (s));
			XP_FREE (s);
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
			  int32 size = this_end - this_start;
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
				  XP_FREE(link);
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
				s = XP_STRDUP(buf);
				if (!s)
				  {
					status = MK_OUT_OF_MEMORY;
					goto FAIL;
				  }
			  }

			if (link)
			  {
				status = MimeHeaders_grow_obuffer (hdrs, XP_STRLEN (link) +
												   XP_STRLEN (s) + 100);
				if (status < 0) goto FAIL;
			  }
			out = hdrs->obuffer;

			if (!show_ids && id_count > 1)
			  {
				XP_STRCPY (out, ", "); out += XP_STRLEN (out);
			  }
			  
			if (link)
			  {
				XP_STRCPY (out, "<A HREF=\""); out += XP_STRLEN (out);
				XP_STRCPY (out, link); out += XP_STRLEN (out);
				XP_STRCPY (out, "\">"); out += XP_STRLEN (out);
			  }
			XP_STRCPY (out, s); out += XP_STRLEN (out);
			XP_FREE (s);
			if (link)
			  {
				XP_STRCPY (out, "</A>"); out += XP_STRLEN (out);
				XP_FREE (link);
			  }
			status = MimeHeaders_write(opt, hdrs->obuffer,
									   XP_STRLEN (hdrs->obuffer));
			if (status < 0) goto FAIL;
		  }

		/* now go around again */
		last_end = this_end;
		this_start = last_end;
	  }
  }

  out = hdrs->obuffer;
  XP_STRCPY (out, HEADER_END_JUNK); out += XP_STRLEN (out);
  *out = 0;
  status = MimeHeaders_write(opt, hdrs->obuffer, out - hdrs->obuffer);
  if (status < 0) goto FAIL;
  else status = 1;

 FAIL:
  return status;
}

static int
MimeHeaders_write_id_header (MimeHeaders *hdrs, const char *name,
							 XP_Bool show_ids, MimeDisplayOptions *opt)
{
  int status = 0;
  char *contents = MimeHeaders_get (hdrs, name, FALSE, TRUE);
  if (!contents) return 0;
  status = MimeHeaders_write_id_header_1 (hdrs, name, contents, show_ids, opt);
  XP_FREE(contents);
  return status;
}



static int MimeHeaders_write_all_headers (MimeHeaders *, MimeDisplayOptions *);
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
  XP_Bool wrote_any_p = FALSE;
  int status = 0;
  const char **rest;
  XP_Bool did_from = FALSE;
  XP_Bool did_resent_from = FALSE;

  status = MimeHeaders_grow_obuffer (hdrs, 200);
  if (status < 0) return status;

  for (rest = mime_interesting_headers; *rest; rest++)
	{
	  const char *name = *rest;

	  /* The Subject header gets written in bold.
	   */
	  if (!strcasecomp(name, HEADER_SUBJECT))
		{
		  status = MimeHeaders_write_subject_header (hdrs, name, opt);
		}

	  /* The References header is never interesting if we're emitting paper,
		 since you can't click on it.
	   */
	  else if (!strcasecomp(name, HEADER_REFERENCES))
		{
		  if (opt->headers != MimeHeadersSomeNoRef)
			status = MimeHeaders_write_id_header (hdrs, name, FALSE, opt);
		}

	  /* Random other Message-ID headers.  These aren't shown by default, but
		 if the user adds them to the list, display them clickably.
	   */
	  else if (!strcasecomp(name, HEADER_MESSAGE_ID) ||
			   !strcasecomp(name, HEADER_RESENT_MESSAGE_ID))
		{
		  status = MimeHeaders_write_id_header (hdrs, name, TRUE, opt);
		}

	  /* The From header supercedes the Sender header.
	   */
	  else if (!strcasecomp(name, HEADER_SENDER) ||
			   !strcasecomp(name, HEADER_FROM))
		{
		  if (did_from) continue;
		  did_from = TRUE;
		  status = MimeHeaders_write_address_header (hdrs, HEADER_FROM, opt);
		  if (status == 0)
			status = MimeHeaders_write_address_header (hdrs, HEADER_SENDER,
													   opt);
		}

	  /* Likewise, the Resent-From header supercedes the Resent-Sender header.
	   */
	  else if (!strcasecomp(name, HEADER_RESENT_SENDER) ||
			   !strcasecomp(name, HEADER_RESENT_FROM))
		{
		  if (did_resent_from) continue;
		  did_resent_from = TRUE;
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
	  else if (!strcasecomp(name, HEADER_REPLY_TO))
		{
		  char *reply_to = MimeHeaders_get (hdrs, HEADER_REPLY_TO,
											FALSE, FALSE);
		  if (reply_to)
			{
			  char *from = MimeHeaders_get (hdrs, HEADER_FROM, FALSE, FALSE);
			  char *froma = (from
							 ? MSG_ExtractRFC822AddressMailboxes(from)
							 : 0);
			  char *repa  = ((reply_to && froma)
							 ? MSG_ExtractRFC822AddressMailboxes(reply_to)
							 : 0);

			  FREEIF(reply_to);
			  if (!froma || !repa || strcasecomp (froma, repa))
				{
				  FREEIF(froma);
				  FREEIF(repa);
				  status = MimeHeaders_write_address_header (hdrs,
															 HEADER_REPLY_TO,
															 opt);
				}
			  FREEIF(repa);
			  FREEIF(froma);
			  FREEIF(from);
			}
		  FREEIF(reply_to);
		}

	  /* Random other address headers.
		 These headers combine all occurences: that is, if there is more than
		 one CC field, all of them will be combined and presented as one.
	   */
	  else if (!strcasecomp(name, HEADER_RESENT_TO) ||
			   !strcasecomp(name, HEADER_RESENT_CC) ||
			   !strcasecomp(name, HEADER_TO) ||
			   !strcasecomp(name, HEADER_CC) ||
			   !strcasecomp(name, HEADER_BCC))
		{
		  status = MimeHeaders_write_address_header (hdrs, name, opt);
		}

	  /* Random other newsgroup headers.
		 These headers combine all occurences, as with address headers.
	   */
	  else if (!strcasecomp(name, HEADER_NEWSGROUPS) ||
			   !strcasecomp(name, HEADER_FOLLOWUP_TO))
		{
		  status = MimeHeaders_write_news_header (hdrs, name, opt);
		}

	  /* Everything else.
		 These headers don't combine: only the first instance of the header
		 will be shown, if there is more than one.
	   */
	  else
		{
		  status = MimeHeaders_write_random_header (hdrs, name, FALSE, opt);
		}

	  if (status < 0) break;
	  wrote_any_p |= (status > 0);
	}

  return (status < 0 ? status : (wrote_any_p ? 1 : 0));
}


static int
MimeHeaders_write_all_headers (MimeHeaders *hdrs, MimeDisplayOptions *opt)
{
  int status;
  int i;
  XP_Bool wrote_any_p = FALSE;

  /* One shouldn't be trying to read headers when one hasn't finished
	 parsing them yet... but this can happen if the message ended
	 prematurely, and has no body at all (as opposed to a null body,
	 which is more normal.)   So, if we try to read from the headers,
	 let's assume that the headers are now finished.  If they aren't
	 in fact finished, then a later attempt to write to them will assert.
   */
  if (!hdrs->done_p)
	{
	  hdrs->done_p = TRUE;
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
	  if (i == 0 && head[0] == 'F' && !XP_STRNCMP(head, "From ", 5))
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

	  name = XP_ALLOC(colon - head + 1);
	  if (!name) return MK_OUT_OF_MEMORY;
	  XP_MEMCPY(name, head, colon - head);
	  name[colon - head] = 0;

	  c2 = XP_ALLOC(end - contents + 1);
	  if (!c2)
		{
		  XP_FREE(name);
		  return MK_OUT_OF_MEMORY;
		}
	  XP_MEMCPY(c2, contents, end - contents);
	  c2[end - contents] = 0;

	  if (!strcasecomp(name, HEADER_CC) ||
		  !strcasecomp(name, HEADER_FROM) ||
		  !strcasecomp(name, HEADER_REPLY_TO) ||
		  !strcasecomp(name, HEADER_RESENT_CC) ||
		  !strcasecomp(name, HEADER_RESENT_FROM) ||
		  !strcasecomp(name, HEADER_RESENT_SENDER) ||
		  !strcasecomp(name, HEADER_RESENT_TO) ||
		  !strcasecomp(name, HEADER_SENDER) ||
		  !strcasecomp(name, HEADER_TO))
		status = MimeHeaders_write_address_header_1(hdrs, name, c2, opt);
	  else if (!strcasecomp(name, HEADER_FOLLOWUP_TO) ||
			   !strcasecomp(name, HEADER_NEWSGROUPS))
		status = MimeHeaders_write_news_header_1(hdrs, name, c2, opt);
	  else if (!strcasecomp(name, HEADER_MESSAGE_ID) ||
			   !strcasecomp(name, HEADER_RESENT_MESSAGE_ID) ||
			   !strcasecomp(name, HEADER_REFERENCES))
		status = MimeHeaders_write_id_header_1(hdrs, name, c2, TRUE, opt);
	  else if (!strcasecomp(name, HEADER_SUBJECT))
		status = MimeHeaders_write_subject_header_1(hdrs, name, c2, opt);

	  else
		status = MimeHeaders_write_random_header_1(hdrs, name, c2, opt, FALSE);
	  XP_FREE(name);
	  XP_FREE(c2);

	  if (status < 0) return status;
	  if (!wrote_any_p) wrote_any_p = (status > 0);
	}

  return (wrote_any_p ? 1 : 0);
}


#ifndef MOZILLA_30
# define EGREGIOUS_HEADERS
#endif

static int
MimeHeaders_write_microscopic_headers (MimeHeaders *hdrs,
									   MimeDisplayOptions *opt)
{
  int status = 1;
  char *subj  = MimeHeaders_get (hdrs, HEADER_SUBJECT, FALSE, FALSE);
  char *from  = MimeHeaders_get (hdrs, HEADER_FROM, FALSE, TRUE);
  char *date  = MimeHeaders_get (hdrs, HEADER_DATE, FALSE, TRUE);
  char *out;

  if (!from)
	from = MimeHeaders_get (hdrs, HEADER_SENDER, FALSE, TRUE);
  if (!date)
	date = MimeHeaders_get (hdrs, HEADER_RESENT_DATE, FALSE, TRUE);

  if (date && opt->reformat_date_fn)
	{
	  char *d2 = opt->reformat_date_fn(date, opt->stream_closure);
	  if (d2)
		{
		  XP_FREE(date);
		  date = d2;
		}
	}


  /* Convert MIME-2 data.
   */
#define FROB(VAR) do {														\
  if (VAR)																	\
	{																		\
	  char *converted = 0;													\
	  int32 converted_length = 0;											\
	  status = MimeHeaders_convert_rfc1522(opt, VAR, XP_STRLEN(VAR),		\
										   &converted, &converted_length);	\
	  if (status < 0) goto FAIL;											\
	  if (converted)														\
		{																	\
		  XP_ASSERT(converted_length == (int32) XP_STRLEN(converted));		\
		  VAR = converted;													\
		}																	\
	} } while(0)
  FROB(from);
  FROB(subj);
  FROB(date);
#undef FROB

#ifndef EGREGIOUS_HEADERS
# define THLHMAAMS 0
#else
# define THLHMAAMS 900
#endif

  status = MimeHeaders_grow_obuffer (hdrs,
									 (subj ? XP_STRLEN(subj)*2 : 0) +
									 (from ? XP_STRLEN(from)   : 0) +
									 (date ? XP_STRLEN(date)   : 0) +
									 100
								 + THLHMAAMS
									 );
  if (status < 0) goto FAIL;

  if (subj) {
	XP_ASSERT(!hdrs->munged_subject);
	hdrs->munged_subject = XP_STRDUP(subj);
  }

  out = hdrs->obuffer;

#ifndef EGREGIOUS_HEADERS
  XP_STRCPY(out, "<B>");
  out += XP_STRLEN(out);
  /* Quotify the subject... */
  if (subj)
	status = NET_ScanForURLs (
# ifndef MOZILLA_30
							  NULL,
# endif /* !MOZILLA_30 */
							  subj, XP_STRLEN(subj), out,
							  hdrs->obuffer_size - (out - hdrs->obuffer) - 10,
							  TRUE);
	if (status < 0) goto FAIL;

  out += XP_STRLEN(out);
  XP_STRCPY(out, "</B> (");
  out += XP_STRLEN(out);

  /* Quotify the sender... */
  if (from)
	status = NET_ScanForURLs (
# ifndef MOZILLA_30
							  NULL,
# endif /* !MOZILLA_30 */
							  from, XP_STRLEN(from), out,
							  hdrs->obuffer_size - (out - hdrs->obuffer) - 10,
							  TRUE);
  if (status < 0) goto FAIL;

  out += XP_STRLEN(out);
  XP_STRCPY(out, ", <NOBR>");
  out += XP_STRLEN(out);

  /* Quotify the date (just in case...) */
  if (date)
	status = NET_ScanForURLs (
# ifndef MOZILLA_30
							  NULL,
# endif /* !MOZILLA_30 */
							  date, XP_STRLEN(date), out,
							  hdrs->obuffer_size - (out - hdrs->obuffer) - 10,
							  TRUE);
  if (status < 0) goto FAIL;

  out += XP_STRLEN(out);
  XP_STRCPY(out, ")</NOBR><BR>");
  out += XP_STRLEN(out);

  status = MimeHeaders_write(opt, hdrs->obuffer, XP_STRLEN(hdrs->obuffer));
  if (status < 0) goto FAIL;

#else  /* EGREGIOUS_HEADERS */
  
  XP_SPRINTF(out, "\
<TR><TD VALIGN=TOP BGCOLOR=\"#CCCCCC\" ALIGN=RIGHT><B>%s: </B></TD>\
<TD VALIGN=TOP BGCOLOR=\"#CCCCCC\" width=100%%>\
<table border=0 cellspacing=0 cellpadding=0 width=100%%><tr>\
<td valign=top>", XP_GetString(MK_MIMEHTML_DISP_FROM));
  out += XP_STRLEN(out);

  if (from) {
	status = MimeHeaders_write(opt, hdrs->obuffer, XP_STRLEN(hdrs->obuffer));
	if (status < 0) goto FAIL;
	status = MimeHeaders_write_grouped_header_1(hdrs, NULL, from, opt, TRUE);
	if (status < 0) goto FAIL;
	out = hdrs->obuffer;
  }

  XP_STRCPY(out, "</td><td valign=top align=right nowrap>");
  out += XP_STRLEN(out);
  if (date) XP_STRCPY(out, date);
  out += XP_STRLEN(out);
  XP_STRCPY(out, "</td></tr></table></TD>");
  out += XP_STRLEN(out);

  /* ### This is where to insert something like <TD VALIGN=TOP align=right bgcolor=\"#CCCCCC\" ROWSPAN=2><IMG SRC=\"http://gooey/projects/dogbert/mail/M_both.gif\"></TD> if we want to do an image. */
  
  XP_STRCPY(out,
			"</TR><TR><TD VALIGN=TOP BGCOLOR=\"#CCCCCC\" ALIGN=RIGHT><B>");
  out += XP_STRLEN(out);
  XP_STRCPY(out, XP_GetString(MK_MIMEHTML_DISP_SUBJECT));
  out += XP_STRLEN(out);
  XP_STRCPY(out, ": </B></TD><TD VALIGN=TOP BGCOLOR=\"#CCCCCC\">");
  out += XP_STRLEN(out);
  if (subj) {
	  status = NET_ScanForURLs(
#ifndef MOZILLA_30
							   NULL,
#endif /* !MOZILLA_30 */
							   subj, XP_STRLEN(subj), out,
							   hdrs->obuffer_size - (out - hdrs->obuffer) - 10,
							   TRUE);
	  if (status < 0) goto FAIL;
  } else {
	  XP_STRCPY(out, "<BR>");
  }
  out += XP_STRLEN(out);
  XP_STRCPY(out, "</TR>");
  out += XP_STRLEN(out);

  status = MimeHeaders_write(opt, hdrs->obuffer, XP_STRLEN(hdrs->obuffer));
  if (status < 0) goto FAIL;

#endif /* EGREGIOUS_HEADERS */


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
	  status = MimeHeaders_write_news_header (hdrs, HEADER_NEWSGROUPS, opt);
	  if (status < 0) goto FAIL;
	}

 FAIL:

  FREEIF(subj);
  FREEIF(from);
  FREEIF(date);

  return (status < 0 ? status : 1);
}


static int
MimeHeaders_write_citation_headers (MimeHeaders *hdrs, MimeDisplayOptions *opt)
{
  int status;
  char *from = 0, *name = 0, *id = 0;
  const char *fmt = 0;
  char *converted = 0;
  int32 converted_length = 0;

  if (!opt || !opt->output_fn)
	return 0;

  from = MimeHeaders_get(hdrs, HEADER_FROM, FALSE, FALSE);
  if (!from)
	from = MimeHeaders_get(hdrs, HEADER_SENDER, FALSE, FALSE);
  if (!from)
	from = XP_STRDUP("Unknown");
  if (!from)
	{
	  status = MK_OUT_OF_MEMORY;
	  goto FAIL;
	}

#if 0
  id = MimeHeaders_get(hdrs, HEADER_MESSAGE_ID, FALSE, FALSE);
#endif

  name = MSG_ExtractRFC822AddressNames (from);
  if (!name)
	{
	  name = from;
	  from = 0;
	}
  FREEIF(from);

  fmt = (id
		 ? XP_GetString(MK_MSG_IN_MSG_X_USER_WROTE)
		 : XP_GetString(MK_MSG_USER_WROTE));

  status = MimeHeaders_grow_obuffer (hdrs,
									 XP_STRLEN(fmt) + XP_STRLEN(name) +
									 (id ? XP_STRLEN(id) : 0) + 58);
  if (status < 0) return status;

#ifndef MOZILLA_30
  if (opt->nice_html_only_p) {
	int32 nReplyWithExtraLines = 0, eReplyOnTop = 0;
	PREF_GetIntPref("mailnews.reply_with_extra_lines", &nReplyWithExtraLines);
	PREF_GetIntPref("mailnews.reply_on_top", &eReplyOnTop);
	if (nReplyWithExtraLines && eReplyOnTop == 1) {
	  for (; nReplyWithExtraLines > 0; nReplyWithExtraLines--) {
		status = MimeHeaders_write(opt, "<BR>", 4);
		if (status < 0) return status;
	  }
	}
  }
#endif

  if (id)
	XP_SPRINTF(hdrs->obuffer, fmt, id, name);
  else
	XP_SPRINTF(hdrs->obuffer, fmt, name);

  status = MimeHeaders_convert_rfc1522(opt, hdrs->obuffer,
									   XP_STRLEN(hdrs->obuffer),
									   &converted, &converted_length);
  if (status < 0) return status;

  if (converted)
	{
	  status = MimeHeaders_write(opt, converted, converted_length);
	  if (status < 0) goto FAIL;
	}
  else
	{
	  status = MimeHeaders_write(opt, hdrs->obuffer, XP_STRLEN(hdrs->obuffer));
	  if (status < 0) goto FAIL;
	}

#ifndef MOZILLA_30
  if (opt->nice_html_only_p) {
	  char ptr[] = "<BLOCKQUOTE TYPE=CITE>";
	  MimeHeaders_write(opt, ptr, XP_STRLEN(ptr));
  }
#endif /* !MOZILLA_30 */

 FAIL:

  FREEIF(from);
  FREEIF(name);
  FREEIF(id);
  return (status < 0 ? status : 1);
}


/* Writes the headers as text/plain.
   This writes out a blank line after the headers, unless
   dont_write_content_type is true, in which case the header-block
   is not closed off, and none of the Content- headers are written.
 */
int
MimeHeaders_write_raw_headers (MimeHeaders *hdrs, MimeDisplayOptions *opt,
							   XP_Bool dont_write_content_type)
{
  int status;

  if (hdrs && !hdrs->done_p)
	{
	  hdrs->done_p = TRUE;
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
	  status = MimeHeaders_write(opt, nl, XP_STRLEN(nl));
	  if (status < 0) return status;
	}
  else if (hdrs)
	{
	  int32 i;
	  for (i = 0; i < hdrs->heads_size; i++)
		{
		  char *head = hdrs->heads[i];
		  char *end = (i == hdrs->heads_size-1
					   ? hdrs->all_headers + hdrs->all_headers_fp
					   : hdrs->heads[i+1]);

		  XP_ASSERT(head);
		  if (!head) continue;

		  /* Don't write out any Content- header. */
		  if (!strncasecomp(head, "Content-", 8))
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
MimeHeaders_write_headers_html (MimeHeaders *hdrs, MimeDisplayOptions *opt)
{
  int status = 0;
  XP_Bool wrote_any_p = FALSE;

  if (!opt || !opt->output_fn) return 0;

  FREEIF(hdrs->munged_subject);

  status = MimeHeaders_grow_obuffer (hdrs, 210);
  if (status < 0) return status;

  if (opt->fancy_headers_p) {
#ifdef JS_ATTACHMENT_MUMBO_JUMBO
	/* First, insert a table for the way-magic javascript appearing attachment
	   indicator.  The ending code for this is way below. */
	XP_STRCPY(hdrs->obuffer, "<TABLE><TR><TD>" MIME_HEADER_TABLE);
#else
	XP_STRCPY(hdrs->obuffer, MIME_HEADER_TABLE);
#endif
  } else
	XP_STRCPY (hdrs->obuffer, "<P>");

  status = MimeHeaders_write(opt, hdrs->obuffer, XP_STRLEN(hdrs->obuffer));
  if (status < 0) return status;

  if (opt->headers == MimeHeadersAll)
	status = MimeHeaders_write_all_headers (hdrs, opt);
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
	  const char *msg = XP_GetString(MK_MSG_NO_HEADERS);
	  XP_STRCPY (hdrs->obuffer, "<TR><TD><B><I>");
	  XP_STRCAT (hdrs->obuffer, msg);
	  XP_STRCAT (hdrs->obuffer, "</I></B></TD></TR>");
	  status = MimeHeaders_write(opt, hdrs->obuffer, XP_STRLEN(hdrs->obuffer));
	  if (status < 0) goto FAIL;
	}

  if (opt->fancy_headers_p) {
	XP_STRCPY (hdrs->obuffer, "</TABLE>");
#ifdef JS_ATTACHMENT_MUMBO_JUMBO
	/* OK, here's the ending code for the way magic javascript indicator. */
	if (!opt->nice_html_only_p && opt->fancy_links_p) {
		if (opt->attachment_icon_layer_id == 0) {
			static int32 randomid = 1; /* Not very random. ### */
			opt->attachment_icon_layer_id = randomid;
			XP_SPRINTF(hdrs->obuffer + XP_STRLEN(hdrs->obuffer),
					   "</TD><TD VALIGN=TOP><LAYER LOCKED name=noattach-%ld>"
					   "</LAYER><ILAYER LOCKED name=attach-%ld visibility=hide>"
					   "<a href=mailbox:displayattachments>"
					   "<IMG SRC=internal-attachment-icon BORDER=0>"
					   "</a></ilayer>",
					   (long) randomid, (long) randomid);
			randomid++;
		}
	}
	XP_STRCAT(hdrs->obuffer, "</td></tr></table>");
#endif
  } else
	XP_STRCPY (hdrs->obuffer, "<P>");

  status = MimeHeaders_write(opt, hdrs->obuffer, XP_STRLEN(hdrs->obuffer));
  if (status < 0) goto FAIL;
  if (hdrs->munged_subject) {
	char* t2 = NET_EscapeHTML(hdrs->munged_subject);
	FREEIF(hdrs->munged_subject);
	if (t2) {
	  status = MimeHeaders_grow_obuffer(hdrs, XP_STRLEN(t2) + 20);
	  if (status >= 0) {
		XP_SPRINTF(hdrs->obuffer, "<TITLE>%s</TITLE>\n", t2);
		status = MimeHeaders_write(opt, hdrs->obuffer,
								   XP_STRLEN(hdrs->obuffer));
	  }
	}
	FREEIF(t2);
	if (status < 0) goto FAIL;
  }



 FAIL:
  MimeHeaders_compact (hdrs);

  return status;
}



/* For drawing the tables that represent objects that can't be displayed
   inline.
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
  char *type = 0, *desc = 0, *enc = 0, *icon = 0, *type_desc = 0;

  type = (content_type
		  ? XP_STRDUP(content_type)
		  : MimeHeaders_get(hdrs, HEADER_CONTENT_TYPE, TRUE, FALSE));

  if (type && *type && opt)
	{
	  if (opt->type_icon_name_fn)
		icon = opt->type_icon_name_fn(type, opt->stream_closure);

	  if (!strcasecomp(type, APPLICATION_OCTET_STREAM))
		type_desc = XP_STRDUP(XP_GetString(MK_MSG_UNSPECIFIED_TYPE));
	  else if (opt->type_description_fn)
		type_desc = opt->type_description_fn(type, opt->stream_closure);
	}

  /* Kludge to use a prettier name for AppleDouble and AppleSingle objects.
   */
  if (type && (!strcasecomp(type, MULTIPART_APPLEDOUBLE) ||
			   !strcasecomp(type, MULTIPART_HEADER_SET) ||
			   !strcasecomp(type, APPLICATION_APPLEFILE)))
	{
	  XP_FREE(type);
	  type = XP_STRDUP(XP_GetString(MK_MSG_MIME_MAC_FILE));
	  FREEIF(icon);
	  icon = XP_STRDUP("internal-gopher-binary");
	}

# define PUT_STRING(S) do { \
		 status = MimeHeaders_grow_obuffer(hdrs, XP_STRLEN(S)+1); \
		 if (status < 0) goto FAIL; \
		 XP_STRCPY (hdrs->obuffer, S); \
		 status = MimeHeaders_write(opt, hdrs->obuffer, \
									XP_STRLEN(hdrs->obuffer)); \
		 if (status < 0) goto FAIL; } while (0)

  PUT_STRING ("<TABLE CELLPADDING=8 CELLSPACING=1 BORDER=1>"
			  "<TR><TD NOWRAP>");

  if (icon)
	{
	  PUT_STRING("<TABLE BORDER=0 CELLPADDING=0 CELLSPACING=0>"
				 "<TR><TD NOWRAP VALIGN=CENTER>");

	  if (lname_url)
		{
		  PUT_STRING("<A HREF=\"");
		  PUT_STRING(lname_url);
		  PUT_STRING("\">");
		}
	  PUT_STRING("<IMG SRC=\"");
	  PUT_STRING(icon);
	  PUT_STRING("\" BORDER=0 ALIGN=MIDDLE ALT=\"\">");

	  if (lname_url)
		PUT_STRING("</A>");

	  PUT_STRING("</TD><TD VALIGN=CENTER>");
	}

  if (lname_url)
	{
	  PUT_STRING("<A HREF=\"");
	  PUT_STRING(lname_url);
	  PUT_STRING("\">");
	}
  if (lname)
	PUT_STRING (lname);
  if (lname_url)
	PUT_STRING("</A>");

  if (icon)
	PUT_STRING("</TD></TR></TABLE>");

  PUT_STRING ("</TD><TD>");

  if (opt->headers == MimeHeadersAll)
	{
	  status = MimeHeaders_write_headers_html (hdrs, opt);
	  if (status < 0) return status;
	}
  else
	{
	  char *name = MimeHeaders_get_name(hdrs);
	  PUT_STRING (MIME_HEADER_TABLE);

	  if (name)
		{
		  const char *name_hdr = MimeHeaders_localize_header_name("Name", opt);
		  PUT_STRING(HEADER_START_JUNK);
		  PUT_STRING(name_hdr);
		  PUT_STRING(HEADER_MIDDLE_JUNK);
		  PUT_STRING(name);
		  FREEIF(name);
		  PUT_STRING(HEADER_END_JUNK);
		}

	  if (type)
		{
		  const char *type_hdr = MimeHeaders_localize_header_name("Type", opt);
		  PUT_STRING(HEADER_START_JUNK);
		  PUT_STRING(type_hdr);
		  PUT_STRING(HEADER_MIDDLE_JUNK);
		  if (type_desc)
			{
			  PUT_STRING(type_desc);
			  PUT_STRING(" (");
			}
		  PUT_STRING(type);
		  if (type_desc)
			PUT_STRING(")");
		  FREEIF(type);
		  FREEIF(type_desc);
		  PUT_STRING(HEADER_END_JUNK);
		}

	  enc = (encoding
			 ? XP_STRDUP(encoding)
			 : MimeHeaders_get(hdrs, HEADER_CONTENT_TRANSFER_ENCODING,
							   TRUE, FALSE));
	  if (enc)
		{
		  const char *enc_hdr = MimeHeaders_localize_header_name("Encoding",
																 opt);
		  PUT_STRING(HEADER_START_JUNK);
		  PUT_STRING(enc_hdr);
		  PUT_STRING(HEADER_MIDDLE_JUNK);
		  PUT_STRING(enc);
		  FREEIF(enc);
		  PUT_STRING(HEADER_END_JUNK);
		}

	  desc = MimeHeaders_get(hdrs, HEADER_CONTENT_DESCRIPTION, FALSE, FALSE);
	  if (!desc)
		{
		  desc = MimeHeaders_get(hdrs, HEADER_X_SUN_DATA_DESCRIPTION,
								 FALSE, FALSE);

		  /* If there's an X-Sun-Data-Description, but it's the same as the
			 X-Sun-Data-Type, don't show it.
		   */
		  if (desc)
			{
			  char *loser = MimeHeaders_get(hdrs, HEADER_X_SUN_DATA_TYPE,
											FALSE, FALSE);
			  if (loser && !strcasecomp(loser, desc))
				FREEIF(desc);
			  FREEIF(loser);
			}
		}

	  if (desc)
		{
		  const char *desc_hdr= MimeHeaders_localize_header_name("Description",
																 opt);
		  PUT_STRING(HEADER_START_JUNK);
		  PUT_STRING(desc_hdr);
		  PUT_STRING(HEADER_MIDDLE_JUNK);
		  PUT_STRING(desc);
		  FREEIF(desc);
		  PUT_STRING(HEADER_END_JUNK);
		}
	  PUT_STRING ("</TABLE>");
	}
  if (body) PUT_STRING(body);
  PUT_STRING ("</TD></TR></TABLE></CENTER>");
# undef PUT_STRING

 FAIL:
  FREEIF(type);
  FREEIF(desc);
  FREEIF(enc);
  FREEIF(icon);
  FREEIF(type_desc);
  MimeHeaders_compact (hdrs);
  return status;
}


/* Some crypto-related HTML-generated utility routines.
 */


char *
MimeHeaders_open_crypto_stamp(void)
{
  return XP_STRDUP("<TABLE CELLPADDING=0 CELLSPACING=0"
				          " WIDTH=\"100%\" BORDER=0>"
				   "<TR VALIGN=TOP><TD WIDTH=\"100%\">");
}

char *
MimeHeaders_finish_open_crypto_stamp(void)
{
  return XP_STRDUP("</TD><TD ALIGN=RIGHT VALIGN=TOP>");
}

char *
MimeHeaders_close_crypto_stamp(void)
{
  return XP_STRDUP("</TD></TR></TABLE><P>");
}


char *
MimeHeaders_make_crypto_stamp(XP_Bool encrypted_p,
							  XP_Bool signed_p,
							  XP_Bool good_p,
							  XP_Bool close_parent_stamp_p,
							  const char *stamp_url)
{
  const char *open = ("%s"
					  "<P>"
				      "<CENTER>"
			            "<TABLE CELLPADDING=3 CELLSPACING=1 BORDER=1>"
			              "<TR>"
			                "<TD ALIGN=RIGHT VALIGN=BOTTOM BGCOLOR=\"white\">"
			                  "%s<IMG SRC=\"%s\" BORDER=0 ALT=\"S/MIME\">%s"
		                      "<B>"
				                "<FONT COLOR=\"red\" SIZE=\"-1\">"
		                          "<BR>"
					  "%s%s%s");
  int16 middle_key = 0;
  char *href_open = 0;
  const char *img_src = "";
  const char *href_close = 0;
  const char *middle = 0;
  const char *close = (         "</FONT>"
					          "</B>"
					        "</TD>"
				          "</TR>"
                        "</TABLE>"
				      "</CENTER>"
					  );
  char *parent_close = 0;
  const char *parent_close_early = 0;
  const char *parent_close_late = 0;
  char *result = 0;

  /* Neither encrypted nor signed means "certs only". */

  if (encrypted_p && signed_p && good_p)				/* 111 */
	{
	  middle_key = MK_MIMEHTML_ENC_AND_SIGNED;
	  img_src = "internal-smime-encrypted-signed";
	}
  else if (!encrypted_p && signed_p && good_p)			/* 011 */
	{
	  middle_key = MK_MIMEHTML_SIGNED;
	  img_src = "internal-smime-signed";
	}
  else if (encrypted_p && !signed_p && good_p)			/* 101 */
	{
	  middle_key = MK_MIMEHTML_ENCRYPTED;
	  img_src = "internal-smime-encrypted";
	}
  else if (!encrypted_p && !signed_p && good_p)			/* 001 */
	{
	  middle_key = MK_MIMEHTML_CERTIFICATES;
	  img_src = "internal-smime-attached";
	}

  else if (encrypted_p && signed_p && !good_p)			/* 110 */
	{
	  middle_key = MK_MIMEHTML_ENC_SIGNED_BAD;
	  img_src = "internal-smime-encrypted-signed-bad";
	}
  else if (!encrypted_p && signed_p && !good_p)			/* 010 */
	{
	  middle_key = MK_MIMEHTML_SIGNED_BAD;
	  img_src = "internal-smime-signed-bad";
	}
  else if (encrypted_p && !signed_p && !good_p)			/* 100 */
	{
	  middle_key = MK_MIMEHTML_ENCRYPTED_BAD;
	  img_src = "internal-smime-encrypted-bad";
	}
  else /* (!encrypted_p && !signed_p && !good_p) */		/* 000 */
	{
	  middle_key = MK_MIMEHTML_CERTIFICATES_BAD;
	  img_src = "internal-smime-signed-bad";
	}

  if (middle_key)
	{
	  middle = XP_GetString(middle_key);

	  /* #### Don't have access to output_csid from here...
		 middle = XP_GetStringForHTML(middle_key, output_csid, (char*)middle);
	   */
	}

  if (close_parent_stamp_p)
	{
	  parent_close = MimeHeaders_close_crypto_stamp();
	  if (!parent_close) goto FAIL; /* MK_OUT_OF_MEMORY */
	}

  parent_close_early = 0;
  parent_close_late = parent_close;
  if (!encrypted_p && !signed_p)
	{
	  /* Kludge for certs-only messages -- close off the parent early
		 so that the stamp goes in the body, not next to the headers. */
	  parent_close_early = parent_close;
	  parent_close_late  = 0;
	}

  if (stamp_url)
	{
	  const char *stamp_text = XP_GetString(MK_MIMEHTML_SHOW_SECURITY_ADVISOR);
	  href_open = PR_smprintf("<A HREF=\"%s\""
							  " onMouseOver=\"window.status='%s';"
							  " return true\">",
							  stamp_url,
							  stamp_text);
	  href_close = "</A>";
	  if (!href_open) goto FAIL; /* MK_OUT_OF_MEMORY */
	}

  result = PR_smprintf(open,
					   (parent_close_early ? parent_close_early : ""),
					    (href_open ? href_open : ""),
					     img_src,
					    (href_close ? href_close : ""),
					    (middle ? middle : ""),
					   close,
					   (parent_close_late ? parent_close_late : ""));
 FAIL:
  FREEIF(parent_close);
  FREEIF(href_open);
  return result;
}

/* Strip CR+LF+<whitespace> runs within (original).
   Since the string at (original) can only shrink,
   this conversion is done in place. (original)
   is returned. */
char *
strip_continuations(char *original)
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

extern int16 INTL_DefaultMailToWinCharSetID(int16 csid);

/* Given text purporting to be a qtext header value, strip backslashes that
	may be escaping other chars in the string. */
char *
mime_decode_filename(char *name)
{
	char *s = name, *d = name;
	char *cvt, *returnVal = NULL;
	int16 mail_csid = CS_DEFAULT, win_csid = CS_DEFAULT;
	
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
	s = XP_STRSTR(returnVal, "=?");
	if (s)
	{
		s += 2;
		d = XP_STRCHR(s, '?');
		if (d) *d = '\0';
		mail_csid = INTL_CharSetNameToID(s);
		if (d) *d = '?';
		win_csid = INTL_DocToWinCharSetID(mail_csid);
		
		cvt = INTL_DecodeMimePartIIStr(returnVal, win_csid, FALSE);
		if (cvt && cvt != returnVal)
			returnVal = cvt;
	}


	/* Seriously ugly hack. If the first three characters of the filename 
	   are <ESC>$B, then we know the filename is in JIS and should be 
	   converted to either SJIS or EUC-JP. */ 
	if ((XP_STRLEN(returnVal) > 3) && 
		(returnVal[0] == 0x1B) && (returnVal[1] == '$') && (returnVal[2] == 'B')) 
	  { 
		int16 dest_csid = INTL_DocToWinCharSetID(CS_JIS); 
		
		cvt = (char *) INTL_ConvertLineWithoutAutoDetect(CS_JIS, dest_csid, (unsigned char *)returnVal, XP_STRLEN(returnVal)); 
		if (cvt && cvt != returnVal) 
		  { 
			if (returnVal != name) XP_FREE(returnVal); 
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

  s = MimeHeaders_get(hdrs, HEADER_CONTENT_DISPOSITION, FALSE, FALSE);
  if (s)
	{
	  name = MimeHeaders_get_parameter(s, HEADER_PARM_FILENAME);
	  XP_FREE(s);
	}

  if (! name)
  {
	  s = MimeHeaders_get(hdrs, HEADER_CONTENT_TYPE, FALSE, FALSE);
	  if (s)
	  {
		  name = MimeHeaders_get_parameter(s, HEADER_PARM_NAME);
		  XP_FREE(s);
	  }
  }

  if (! name)
	  name = MimeHeaders_get (hdrs, HEADER_CONTENT_NAME, FALSE, FALSE);
  
  if (! name)
	  name = MimeHeaders_get (hdrs, HEADER_X_SUN_DATA_NAME, FALSE, FALSE);

  if (name)
  {
		/*	First remove continuation delimiters (CR+LF+space), then
			remove escape ('\\') characters, then attempt to decode
			mime-2 encoded-words. The latter two are done in 
			mime_decode_filename. 
			*/
		strip_continuations(name);

		/*	Argh. What we should do if we want to be robust is to decode qtext
			in all appropriate headers. Unfortunately, that would be too scary
			at this juncture. So just decode qtext/mime2 here. */
	   	cvt = mime_decode_filename(name);
	   	if (cvt && cvt != name)
	   	{
	   		XP_FREE(name);
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
		cmd = XP_STRDUP(cmd);
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
