/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/*   msgutils.c --- various and sundry
 */

#include "msg.h"
#include "xp_time.h"
#include "xpgetstr.h"
#include "xplocale.h"
#include "htmldlgs.h"
#include "prefapi.h"
#include "xp_qsort.h"

extern int MK_OUT_OF_MEMORY;
extern int MK_MSG_NON_MAIL_FILE_WRITE_QUESTION;
extern int MK_MSG_HTML_DOMAINS_DIALOG;
extern int MK_MSG_HTML_DOMAINS_DIALOG_TITLE;

#ifdef XP_MAC
#pragma warn_unusedarg off
#endif

int
msg_GetURL(MWContext* context, URL_Struct* url, XP_Bool issafe)
{
  XP_ASSERT(context);

  /* phil & bienvenu think issafe means "allowed to start another URL even if
     one is already runing". e.g. delete from msgPane, and load next msg */
  if (!issafe)
	msg_InterruptContext (context, TRUE);
  
  url->internal_url = TRUE;
  if (!url->open_new_window_specified)
  {
	  url->open_new_window_specified = TRUE;
	  url->open_new_window = FALSE;
  }

  return FE_GetURL(context, url);
}


void
msg_InterruptContext(MWContext* context, XP_Bool safetoo)
{
  XP_InterruptContext(context);


#ifdef NOTDEF					/* ###tw */

  struct MSG_Frame *msg_frame;  
  if (!context) return;
  if (safetoo || !context->msgframe ||  
	  !context->msgframe->safe_background_activity) {
	/* save msg_frame in case context gets deleted on interruption */
	msg_frame = context->msgframe;
	XP_InterruptContext(context);
	if (msg_frame) {  
	  msg_frame->safe_background_activity = FALSE;
	}  
  }
#endif
}


/* Buffer management.
   Why do I feel like I've written this a hundred times before?
 */
int
msg_GrowBuffer (uint32 desired_size, uint32 element_size, uint32 quantum,
				char **buffer, uint32 *size)
{
  if (*size <= desired_size)
	{
	  char *new_buf;
	  uint32 increment = desired_size - *size;
	  if (increment < quantum) /* always grow by a minimum of N bytes */
		increment = quantum;

#ifdef TESTFORWIN16
	  if (((*size + increment) * (element_size / sizeof(char))) >= 64000)
		{
		  /* Be safe on WIN16 */
		  XP_ASSERT(0);
		  return MK_OUT_OF_MEMORY;
		}
#endif /* DEBUG */

	  new_buf = (*buffer
				 ? (char *) XP_REALLOC (*buffer, (*size + increment)
										* (element_size / sizeof(char)))
				 : (char *) XP_ALLOC ((*size + increment)
									  * (element_size / sizeof(char))));
	  if (! new_buf)
		return MK_OUT_OF_MEMORY;
	  *buffer = new_buf;
	  *size += increment;
	}
  return 0;
}


extern XP_Bool NET_POP3TooEarlyForEnd(int32 len);

/* Take the given buffer, tweak the newlines at the end if necessary, and
   send it off to the given routine.  We are guaranteed that the given
   buffer has allocated space for at least one more character at the end. */
static int
msg_convert_and_send_buffer(char* buf, uint32 length, XP_Bool convert_newlines_p,
							int32 (*per_line_fn) (char *line,
												  uint32 line_length,
												  void *closure),
							void *closure)
{
  /* Convert the line terminator to the native form.
   */
  char* newline;

  XP_ASSERT(buf && length > 0);
  if (!buf || length <= 0) return -1;
  newline = buf + length;

  XP_ASSERT(newline[-1] == CR || newline[-1] == LF);
  if (newline[-1] != CR && newline[-1] != LF) return -1;

  NET_POP3TooEarlyForEnd(length);		/* update count of bytes parsed adding/removing CR or LF*/

  if (!convert_newlines_p)
	{
	}
#if (LINEBREAK_LEN == 1)
  else if ((newline - buf) >= 2 &&
		   newline[-2] == CR &&
		   newline[-1] == LF)
	{
	  /* CRLF -> CR or LF */
	  buf [length - 2] = LINEBREAK[0];
	  length--;
	}
  else if (newline > buf + 1 &&
		   newline[-1] != LINEBREAK[0])
	{
	  /* CR -> LF or LF -> CR */
	  buf [length - 1] = LINEBREAK[0];
	}
#else
  else if (((newline - buf) >= 2 && newline[-2] != CR) ||
		   ((newline - buf) >= 1 && newline[-1] != LF))
	{
	  /* LF -> CRLF or CR -> CRLF */
	  length++;
	  buf[length - 2] = LINEBREAK[0];
	  buf[length - 1] = LINEBREAK[1];
	}
#endif

  return (*per_line_fn)(buf, length, closure);
}


/* SI::BUFFERED-STREAM-MIXIN
   Why do I feel like I've written this a hundred times before?
 */


int
msg_LineBuffer (const char *net_buffer, int32 net_buffer_size,
				char **bufferP, uint32 *buffer_sizeP, uint32 *buffer_fpP,
				XP_Bool convert_newlines_p,
				int32 (*per_line_fn) (char *line, uint32 line_length,
									  void *closure),
				void *closure)
{
  int status = 0;
  if (*buffer_fpP > 0 && *bufferP && (*bufferP)[*buffer_fpP - 1] == CR &&
	  net_buffer_size > 0 && net_buffer[0] != LF) {
	/* The last buffer ended with a CR.  The new buffer does not start
	   with a LF.  This old buffer should be shipped out and discarded. */
	XP_ASSERT(*buffer_sizeP > *buffer_fpP);
	if (*buffer_sizeP <= *buffer_fpP) return -1;
	status = msg_convert_and_send_buffer(*bufferP, *buffer_fpP,
										 convert_newlines_p,
										 per_line_fn, closure);
	if (status < 0) return status;
	*buffer_fpP = 0;
  }
  while (net_buffer_size > 0)
	{
	  const char *net_buffer_end = net_buffer + net_buffer_size;
	  const char *newline = 0;
	  const char *s;


	  for (s = net_buffer; s < net_buffer_end; s++)
		{
		  /* Move forward in the buffer until the first newline.
			 Stop when we see CRLF, CR, or LF, or the end of the buffer.
			 *But*, if we see a lone CR at the *very end* of the buffer,
			 treat this as if we had reached the end of the buffer without
			 seeing a line terminator.  This is to catch the case of the
			 buffers splitting a CRLF pair, as in "FOO\r\nBAR\r" "\nBAZ\r\n".
		   */
		  if (*s == CR || *s == LF)
			{
			  newline = s;
			  if (newline[0] == CR)
				{
				  if (s == net_buffer_end - 1)
					{
					  /* CR at end - wait for the next character. */
					  newline = 0;
					  break;
					}
				  else if (newline[1] == LF)
					/* CRLF seen; swallow both. */
					newline++;
				}
			  newline++;
			  break;
			}
		}

	  /* Ensure room in the net_buffer and append some or all of the current
		 chunk of data to it. */
	  {
		const char *end = (newline ? newline : net_buffer_end);
		uint32 desired_size = (end - net_buffer) + (*buffer_fpP) + 1;

		if (desired_size >= (*buffer_sizeP))
		  {
			status = msg_GrowBuffer (desired_size, sizeof(char), 1024,
									 bufferP, buffer_sizeP);
			if (status < 0) return status;
		  }
		XP_MEMCPY ((*bufferP) + (*buffer_fpP), net_buffer, (end - net_buffer));
		(*buffer_fpP) += (end - net_buffer);
	  }

	  /* Now *bufferP contains either a complete line, or as complete
		 a line as we have read so far.

		 If we have a line, process it, and then remove it from `*bufferP'.
		 Then go around the loop again, until we drain the incoming data.
	   */
	  if (!newline)
		return 0;

	  status = msg_convert_and_send_buffer(*bufferP, *buffer_fpP,
										   convert_newlines_p,
										   per_line_fn, closure);
	  if (status < 0) return status;

	  net_buffer_size -= (newline - net_buffer);
	  net_buffer = newline;
	  (*buffer_fpP) = 0;
	}
  return 0;
}


/* The opposite of msg_LineBuffer(): takes small buffers and packs them
   up into bigger buffers before passing them along.

   Pass in a desired_buffer_size 0 to tell it to flush (for example, in
   in the very last call to this function.)
 */
int
msg_ReBuffer (const char *net_buffer, int32 net_buffer_size,
			  uint32 desired_buffer_size,
			  char **bufferP, uint32 *buffer_sizeP, uint32 *buffer_fpP,
			  int32 (*per_buffer_fn) (char *buffer, uint32 buffer_size,
									  void *closure),
			  void *closure)
{
  int status = 0;

  if (desired_buffer_size >= (*buffer_sizeP))
	{
	  status = msg_GrowBuffer (desired_buffer_size, sizeof(char), 1024,
							   bufferP, buffer_sizeP);
	  if (status < 0) return status;
	}

  do
	{
	  int32 size = *buffer_sizeP - *buffer_fpP;
	  if (size > net_buffer_size)
		size = net_buffer_size;
	  if (size > 0)
		{
		  XP_MEMCPY ((*bufferP) + (*buffer_fpP), net_buffer, size);
		  (*buffer_fpP) += size;
		  net_buffer += size;
		  net_buffer_size -= size;
		}

	  if (*buffer_fpP > 0 &&
		  *buffer_fpP >= desired_buffer_size)
		{
		  status = (*per_buffer_fn) ((*bufferP), (*buffer_fpP), closure);
		  *buffer_fpP = 0;
		  if (status < 0) return status;
		}
	}
  while (net_buffer_size > 0);

  return 0;
}


struct msg_rebuffering_stream_data
{
  uint32 desired_size;
  char *buffer;
  uint32 buffer_size;
  uint32 buffer_fp;
  NET_StreamClass *next_stream;
};


static int32
msg_rebuffering_stream_write_next_chunk (char *buffer, uint32 buffer_size,
										 void *closure)
{
  struct msg_rebuffering_stream_data *sd =
	(struct msg_rebuffering_stream_data *) closure;
  XP_ASSERT (sd);
  if (!sd) return -1;
  if (!sd->next_stream) return -1;
  return (*sd->next_stream->put_block) (sd->next_stream,
										buffer, buffer_size);
}

static int
msg_rebuffering_stream_write_chunk (NET_StreamClass *stream,
									const char* net_buffer,
									int32 net_buffer_size)
{
  struct msg_rebuffering_stream_data *sd =
	(struct msg_rebuffering_stream_data *) stream->data_object;  
  XP_ASSERT (sd);
  if (!sd) return -1;
  return msg_ReBuffer (net_buffer, net_buffer_size,
					   sd->desired_size,
					   &sd->buffer, &sd->buffer_size, &sd->buffer_fp,
					   msg_rebuffering_stream_write_next_chunk,
					   sd);
}


extern XP_Bool ValidateDocData(MWContext *window_id);

static void
msg_rebuffering_stream_abort (NET_StreamClass *stream, int status)
{
  struct msg_rebuffering_stream_data *sd =
	(struct msg_rebuffering_stream_data *) stream->data_object;  
  if (!sd) return;
  FREEIF (sd->buffer);
  if (sd->next_stream)
	{
	  if (ValidateDocData(sd->next_stream->window_id)) /* otherwise doc_data is gone !  */
		(*sd->next_stream->abort) (sd->next_stream, status);
	  XP_FREE (sd->next_stream);
	}
  XP_FREE (sd);
}

static void
msg_rebuffering_stream_complete (NET_StreamClass *stream)
{
  struct msg_rebuffering_stream_data *sd =
	(struct msg_rebuffering_stream_data *) stream->data_object;  
  if (!sd) return;
  sd->desired_size = 0;
  msg_rebuffering_stream_write_chunk (stream, "", 0);
  FREEIF (sd->buffer);
  if (sd->next_stream)
	{
	  (*sd->next_stream->complete) (sd->next_stream);
	  XP_FREE (sd->next_stream);
	}
  XP_FREE (sd);
}

static unsigned int
msg_rebuffering_stream_write_ready (NET_StreamClass *stream)
{
  struct msg_rebuffering_stream_data *sd =
	(struct msg_rebuffering_stream_data *) stream->data_object;  
  if (sd && sd->next_stream)
    return ((*sd->next_stream->is_write_ready)
			(sd->next_stream));
  else
    return (MAX_WRITE_READY);
}

NET_StreamClass * 
msg_MakeRebufferingStream (NET_StreamClass *next_stream,
						   URL_Struct *url,
						   MWContext *context)
{
  NET_StreamClass *stream;
  struct msg_rebuffering_stream_data *sd;

  XP_ASSERT (next_stream);

  TRACEMSG(("Setting up rebuffering stream. Have URL: %s\n", url->address));

  stream = XP_NEW (NET_StreamClass);
  if (!stream) return 0;

  sd = XP_NEW (struct msg_rebuffering_stream_data);
  if (! sd)
	{
	  XP_FREE (stream);
	  return 0;
	}

  XP_MEMSET (sd, 0, sizeof(*sd));
  XP_MEMSET (stream, 0, sizeof(*stream));

  sd->next_stream = next_stream;
  sd->desired_size = 10240;

  stream->name           = "ReBuffering Stream";
  stream->complete       = msg_rebuffering_stream_complete;
  stream->abort          = msg_rebuffering_stream_abort;
  stream->put_block      = msg_rebuffering_stream_write_chunk;
  stream->is_write_ready = msg_rebuffering_stream_write_ready;
  stream->data_object    = sd;
  stream->window_id      = context;

  return stream;
}



XP_Bool
MSG_RequiresComposeWindow (const char *url)
{
  if (!url) return FALSE;
  if (!strncasecomp (url, "mailto:", 7))
	{
	  return TRUE;
	}
  return FALSE;
}


XP_Bool
MSG_RequiresBrowserWindow (const char *url)
{
  if (!url) return FALSE;
  if (MSG_RequiresNewsWindow (url) ||
	  MSG_RequiresMailWindow (url) ||
	  !strncasecomp (url, "about:", 6) ||
	  !strncasecomp (url, "addbook:", 8) ||
	  !strncasecomp (url, "addbook-ldap", 12) || /* no colon so addbook-ldap and addbook-ldaps both match */
	  !strncasecomp (url, "mailto:", 7) ||
	  !strncasecomp (url, "view-source:", 12) ||
	  !strncasecomp (url, "internal-callback-handler:", 26) ||
	  !strncasecomp (url, "internal-panel-handler", 22) ||
	  !strncasecomp (url, "internal-dialog-handler", 23))
	return FALSE;

  else if (!strncasecomp (url, "news:", 5) ||
		   !strncasecomp (url, "snews:", 6) ||
		   !strncasecomp (url, "mailbox:", 8) ||
		   !strncasecomp (url, "IMAP:", 5))
	{
	  /* Mail and news messages themselves don't require browser windows,
		 but their attachments do. */
	  if (XP_STRSTR(url, "?part=") || XP_STRSTR(url, "&part="))
		return TRUE;
	  else
		return FALSE;
	}
  else
	return TRUE;
}



/* If we're in a mail window, and clicking on a link which will itself
   require a mail window, then don't allow this to show up in a different
   window - since there can only be one mail window.
 */
XP_Bool
MSG_NewWindowProhibited (MWContext *context, const char *url)
{
  if (!context) return FALSE;
  if ((context->type == MWContextMail &&
       MSG_RequiresMailWindow (url)) ||
      (context->type == MWContextNews &&
       MSG_RequiresNewsWindow (url)) ||
      (MSG_RequiresComposeWindow (url)))
    return TRUE;
  else
    return FALSE;
}


char *
MSG_ConvertToQuotation (const char *string)
{
  int32 column = 0;
  int32 newlines = 0;
  int32 chars = 0;
  const char *in;
  char *out;
  char *new_string;

  if (! string) return 0;

  /* First, count up the lines in the string. */
  for (in = string; *in; in++)
    {
	  chars++;
      if (*in == CR || *in == LF)
		{
		  if (in[0] == CR && in[1] == LF) {
		 	 in++;
		 	 chars++;
		  }
		  newlines++;
		  column = 0;
		}
      else
		{
		  column++;
		}
    }
  /* If the last line doesn't end in a newline, pretend it does. */
  if (column != 0)
    newlines++;

  /* 2 characters for each '> ', +1 for '\0', and + potential linebreak */
  new_string = (char *) XP_ALLOC (chars + (newlines * 2) + 1 + LINEBREAK_LEN);
  if (! new_string)
	return 0;

  column = 0;
  out = new_string;

  /* Now copy. */
  for (in = string; *in; in++)
    {
	  if (column == 0)
		{
		  *out++ = '>';
		  *out++ = ' ';
		}
		
	  *out++ = *in;
      if (*in == CR || *in == LF)
		{
		  if (in[0] == CR && in[1] == LF)
			*out++ = *++in;
		  newlines++;
		  column = 0;
		}
      else
		{
		  column++;
		}
    }

 /* If the last line doesn't end in a newline, add one. */
	if (column != 0)
  	{
		XP_STRCPY (out, LINEBREAK);
		out += LINEBREAK_LEN;
  	}
  
 	*out = 0;

  return new_string;
}



/* Given a string and a length, removes any "Re:" strings from the front.
   It also deals with that "Re[2]:" thing that some mailers do.

   Returns TRUE if it made a change, FALSE otherwise.

   The string is not altered: the pointer to its head is merely advanced,
   and the length correspondingly decreased.
 */
XP_Bool
msg_StripRE(const char **stringP, uint32 *lengthP)
{
  const char *s, *s_end;
  const char *last;
  uint32 L;
  XP_Bool result = FALSE;
  XP_ASSERT(stringP);
  if (!stringP) return FALSE;
  s = *stringP;
  L = lengthP ? *lengthP : XP_STRLEN(s);

  s_end = s + L;
  last = s;

 AGAIN:

  while (s < s_end && XP_IS_SPACE(*s))
	s++;

  if (s < (s_end-2) &&
	  (s[0] == 'r' || s[0] == 'R') &&
	  (s[1] == 'e' || s[1] == 'E'))
	{
	  if (s[2] == ':')
		{
		  s = s+3;			/* Skip over "Re:" */
		  result = TRUE;	/* Yes, we stripped it. */
		  goto AGAIN;		/* Skip whitespace and try again. */
		}
	  else if (s[2] == '[' || s[2] == '(')
		{
		  const char *s2 = s+3;		/* Skip over "Re[" */

		  /* Skip forward over digits after the "[". */
		  while (s2 < (s_end-2) && XP_IS_DIGIT(*s2))
			s2++;

		  /* Now ensure that the following thing is "]:"
			 Only if it is do we alter `s'.
		   */
		  if ((s2[0] == ']' || s2[0] == ')') && s2[1] == ':')
			{
			  s = s2+2;			/* Skip over "]:" */
			  result = TRUE;	/* Yes, we stripped it. */
			  goto AGAIN;		/* Skip whitespace and try again. */
			}
		}
	}

  /* Decrease length by difference between current ptr and original ptr.
	 Then store the current ptr back into the caller. */
  if (lengthP) *lengthP -= (s - (*stringP));
  *stringP = s;

  return result;
}




char*
msg_GetDummyEnvelope(void)
{
  static char result[75];
  char *ct;
  time_t now = time ((time_t *) 0);
#if defined (XP_WIN)
  if (now < 0 || now > 0x7FFFFFFF)
	  now = 0x7FFFFFFF;
#endif
  ct = ctime(&now);
  XP_ASSERT(ct[24] == CR || ct[24] == LF);
  ct[24] = 0;
  /* This value must be in ctime() format, with English abbreviations.
	 strftime("... %c ...") is no good, because it is localized. */
  XP_STRCPY(result, "From - ");
  XP_STRCPY(result + 7, ct);
  XP_STRCPY(result + 7 + 24, LINEBREAK);
  return result;
}

/* #define STRICT_ENVELOPE */

XP_Bool
msg_IsEnvelopeLine(const char *buf, int32 buf_size)
{
#ifdef STRICT_ENVELOPE
  /* The required format is
	   From jwz  Fri Jul  1 09:13:09 1994
	 But we should also allow at least:
	   From jwz  Fri, Jul 01 09:13:09 1994
	   From jwz  Fri Jul  1 09:13:09 1994 PST
	   From jwz  Fri Jul  1 09:13:09 1994 (+0700)

	 We can't easily call XP_ParseTimeString() because the string is not
	 null terminated (ok, we could copy it after a quick check...) but
	 XP_ParseTimeString() may be too lenient for our purposes.

	 DANGER!!  The released version of 2.0b1 was (on some systems,
	 some Unix, some NT, possibly others) writing out envelope lines
	 like "From - 10/13/95 11:22:33" which STRICT_ENVELOPE will reject!
   */
  const char *date, *end;

  if (buf_size < 29) return FALSE;
  if (*buf != 'F') return FALSE;
  if (XP_STRNCMP(buf, "From ", 5)) return FALSE;

  end = buf + buf_size;
  date = buf + 5;

  /* Skip horizontal whitespace between "From " and user name. */
  while ((*date == ' ' || *date == '\t') && date < end)
	date++;

  /* If at the end, it doesn't match. */
  if (XP_IS_SPACE(*date) || date == end)
	return FALSE;

  /* Skip over user name. */
  while (!XP_IS_SPACE(*date) && date < end)
	date++;

  /* Skip horizontal whitespace between user name and date. */
  while ((*date == ' ' || *date == '\t') && date < end)
	date++;

  /* Don't want this to be localized. */
# define TMP_ISALPHA(x) (((x) >= 'A' && (x) <= 'Z') || \
						 ((x) >= 'a' && (x) <= 'z'))

  /* take off day-of-the-week. */
  if (date >= end - 3)
	return FALSE;
  if (!TMP_ISALPHA(date[0]) || !TMP_ISALPHA(date[1]) || !TMP_ISALPHA(date[2]))
	return FALSE;
  date += 3;
  /* Skip horizontal whitespace (and commas) between dotw and month. */
  if (*date != ' ' && *date != '\t' && *date != ',')
	return FALSE;
  while ((*date == ' ' || *date == '\t' || *date == ',') && date < end)
	date++;

  /* take off month. */
  if (date >= end - 3)
	return FALSE;
  if (!TMP_ISALPHA(date[0]) || !TMP_ISALPHA(date[1]) || !TMP_ISALPHA(date[2]))
	return FALSE;
  date += 3;
  /* Skip horizontal whitespace between month and dotm. */
  if (date == end || (*date != ' ' && *date != '\t'))
	return FALSE;
  while ((*date == ' ' || *date == '\t') && date < end)
	date++;

  /* Skip over digits and whitespace. */
  while (((*date >= '0' && *date <= '9') || *date == ' ' || *date == '\t') &&
		 date < end)
	date++;
  /* Next character should be a colon. */
  if (date >= end || *date != ':')
	return FALSE;

  /* Ok, that ought to be enough... */

# undef TMP_ISALPHA

#else  /* !STRICT_ENVELOPE */

  if (buf_size < 5) return FALSE;
  if (*buf != 'F') return FALSE;
  if (XP_STRNCMP(buf, "From ", 5)) return FALSE;

#endif /* !STRICT_ENVELOPE */

  return TRUE;
}



XP_Bool
msg_ConfirmMailFile (MWContext *context, const char *file_name)
{
  XP_File in = XP_FileOpen (file_name, xpMailFolder, XP_FILE_READ_BIN);
  char buf[100];
  char *s = buf;
  int L;
  if (!in) return TRUE;
  L = XP_FileRead(buf, sizeof(buf)-2, in);
  XP_FileClose (in);
  if (L < 1) return TRUE;
  buf[L] = 0;
  while (XP_IS_SPACE(*s))
	s++, L--;
  if (L > 5 && msg_IsEnvelopeLine(s, L))
	return TRUE;
  PR_snprintf (buf, sizeof(buf),
			   XP_GetString (MK_MSG_NON_MAIL_FILE_WRITE_QUESTION), file_name);
  return FE_Confirm (context, buf);
}


void msg_ClearMessageArea(MWContext* context)
{
  /* ###tw  This needs to be replaced by stuff that notifies FE's the
	 right way */
#ifndef _USRDLL
  /* hack.  Open a stream to layout, give it
	 whitespace (it needs something) and then close it
	 right away.  This has the effect of getting the front end to
	 clear out the HTML display area.
   */
	NET_StreamClass *stream;
	static PA_InitData data;
	URL_Struct *url;
	if (!context)
		return;

	data.output_func = LO_ProcessTag;
	url = NET_CreateURLStruct ("", NET_NORMAL_RELOAD);
	if (!url) return;
	stream = PA_BeginParseMDL (FO_PRESENT, &data, url, context);
	if (stream)
	{
		char buf[] = "<BODY></BODY>";
		int status = (*stream->put_block) (stream, buf, 13);
		if (status < 0)
			(*stream->abort) (stream, status);
		else
			(*stream->complete) (stream);
		XP_FREE (stream);
	}
	NET_FreeURLStruct (url);
	FE_SetProgressBarPercent (context, 0);
#endif
/* MSG_LoadMessage, with MSG_MESSAGEKEYNONE calls msg_ClearMessageArea, which 
// has a bug (drawing the background in grey).  So I just changed MSG_LoadMessage() to 
// do nothing for XP_MAC except set its m_Key to MSG_MESSAGEKEYNONE and exit.  This transfers 
// the responsibility for clearing the message area to the front end.  So far, so good. 
// 
// Now, it's no good just painting the area, because the next refresh will redraw using the 
// existing history entry.  So somehow we have to remove the history entry. 
// There's no API for doing this, except calling SHIST_AddDocument with an entry whose 
// address string is null or empty. 
// 
// If this state of affairs changes, this code will break, but I put in asserts to 
// notify us about it. 98/01/21 
// --------------------------------------------------------------------------- 
*/
#if 0
	/* It would be nice to do it this way, but we need to cause a refresh in the fe */
	URL_Struct* url = NET_CreateURLStruct("", NET_NORMAL_RELOAD);
	History_entry* newEntry; 
	LO_DiscardDocument(context);
	XP_ASSERT(url);
	newEntry = SHIST_CreateHistoryEntry(url, "Nobody Home");
	XP_FREEIF(url);
	XP_ASSERT(newEntry);
	/* Using an empty address string will cause "AddDocument" to do a removal of the old entry,
	 // then delete the new entry, and exit.
 */
	SHIST_AddDocument(context, newEntry);
#endif
}

static MSG_HEADER_SET standard_header_set[] = {
    MSG_TO_HEADER_MASK,
    MSG_REPLY_TO_HEADER_MASK,
    MSG_CC_HEADER_MASK,
    MSG_BCC_HEADER_MASK,
    MSG_NEWSGROUPS_HEADER_MASK,
    MSG_FOLLOWUP_TO_HEADER_MASK
    };

#define TOTAL_HEADERS (sizeof(standard_header_set)/sizeof(MSG_HEADER_SET))

extern int MSG_ExplodeHeaderField(MSG_HEADER_SET msg_header, const char * field, MSG_HeaderEntry ** return_list)
{
  XP_ASSERT(return_list);
  *return_list = NULL;
  if (field && strlen(field)) {
    MSG_HeaderEntry *list=NULL;
    char * name;
    char * address ;
    int count;

    count = MSG_ParseRFC822Addresses (field, &name, &address);
    if (count > 0) {
      char * address_start = address;
      char * name_start = name;
      int i;

      list = (MSG_HeaderEntry*)XP_ALLOC(sizeof(MSG_HeaderEntry)*count);
      if (!list)
        return(-1);

      for (i=0; i<count; i++) {
        list[i].header_type = msg_header;
        list[i].header_value = XP_STRDUP(address);
        if (name && strlen(name))
          list[i].header_value = PR_smprintf("%s <%s>", name, address);
        else
          list[i].header_value = XP_STRDUP(address);
        while (*address != '\0')
          address++;
        address++;
        while (*name != '\0')
          name++;
        name++;
      }
      if (name)
        XP_FREE(name_start);
      if (address)
        XP_FREE(address_start);
    }
    *return_list = list;
    return count;
  }
  return(0);
}

extern int MSG_CompressHeaderEntries( MSG_HeaderEntry * in_list, int list_size, MSG_HeaderEntry ** return_list)
{
  int total = 0;
  *return_list = NULL;
  if (in_list != NULL && list_size > 0) {
    MSG_HeaderEntry * list = NULL;
    char * new_header_value;
    int i,j;

    for (i=0; i<TOTAL_HEADERS; i++) {
      new_header_value = NULL;
      for (j=0; j<list_size; j++) {
	    if (in_list[j].header_type == standard_header_set[i]) {
		  XP_Bool zero_init = FALSE;
		  int header_length = 0;
		  if (!new_header_value)
		    zero_init = TRUE;
          else
		    header_length = XP_STRLEN(new_header_value)+1;
          if (XP_STRLEN(in_list[j].header_value) == 0)
              continue;
		  new_header_value = (char*)XP_REALLOC(
		    new_header_value,
            header_length+XP_STRLEN(in_list[j].header_value)+XP_STRLEN(",")+1);
		  if (new_header_value == NULL) {
		    if (list != NULL) 
			  XP_FREE(list);
              /* don't forget to free up previous header_value entries */
            return(-1);
          }
          if (zero_init)
            new_header_value[0]='\0';
          if (new_header_value && XP_STRLEN(new_header_value))
		    XP_STRCAT(new_header_value,",");
          XP_STRCAT(new_header_value,in_list[j].header_value);
        }
      }
      if (new_header_value) {
        total++;
        list = (MSG_HeaderEntry *)XP_REALLOC(list,total*sizeof(MSG_HeaderEntry));
	    if (list==NULL) {
	      if (new_header_value)
	       XP_FREE(new_header_value);
          return(-1);
        }
        list[total-1].header_type = standard_header_set[i];
        list[total-1].header_value = new_header_value;
      }
    }
    *return_list = list;
  }
  return(total);
}



static int
msg_qsort_domains(const void* a, const void* b)
{
	return XP_STRCMP(*((const char**) a), *((const char**) b));
}


static int
msg_generate_domains_list(char* domainlist, int* numfound, char*** returnlist)
{
	int num;
	int i;
	int j;
	char* ptr;
	char* endptr;
	char** list = NULL;
	for (num=0 , ptr = domainlist ; ptr ; num++, ptr = endptr) {
		endptr = XP_STRCHR(ptr, ',');
		if (endptr) endptr++;
	}
	if (num > 0) {
		list = (char**) XP_CALLOC(num, sizeof(char*));
		if (!list) return MK_OUT_OF_MEMORY;
		for (i=0 , ptr = domainlist ; ptr ; i++, ptr = endptr) {
			endptr = XP_STRCHR(ptr, ',');
			if (endptr) *endptr++ = '\0';
			XP_ASSERT(i < num);
			if (i < num) list[i] = ptr;
		}
		XP_QSORT(list, num, sizeof(char*), msg_qsort_domains);
		/* Now, remove any empty entries or duplicates. */
		for (i=0, j=0 ; i<num ; i++) {
			ptr = list[i];
			if (ptr && (j == 0 || XP_STRCMP(ptr, list[j - 1]) != 0)) {
				list[j++] = ptr;
			}
		}
		num = j;
	}
	*numfound = num;
	*returnlist = list;
	return 0;
}


static PRBool
HTMLDomainsDialogDone(XPDialogState* state, char** argv, int argc,
					  unsigned int button)
{
	char* domainlist = NULL;
	char* gone;
	char* ptr;
	char* endptr;
	char** list = NULL;
	int num;
	int i;
	int status;

	if (button != XP_DIALOG_OK_BUTTON) return PR_FALSE;
	PREF_CopyCharPref("mail.htmldomains", &domainlist);
	if (!domainlist || !*domainlist) return PR_FALSE;
	gone = XP_FindValueInArgs("gone", argv, argc);
	XP_ASSERT(gone);
	if (!gone || !*gone) return PR_FALSE;

	status = msg_generate_domains_list(domainlist, &num, &list);
	if (status < 0) goto FAIL;

	for (ptr = gone ; ptr ; ptr = endptr) {
		endptr = XP_STRCHR(ptr, ',');
		if (endptr) *endptr++ = '\0';
		if (*ptr) {
			i = atoi(ptr);
			XP_ASSERT(i >= 0 && i < num);
			if (i >= 0 && i < num) {
				XP_ASSERT(list[i]);
				list[i] = NULL;
			}
		}
	}
	ptr = NULL;
	for (i=0 ; i<num ; i++) {
		if (list[i]) {
			if (ptr) StrAllocCat(ptr, ",");
			StrAllocCat(ptr, list[i]);
		}
	}
	PREF_SetCharPref("mail.htmldomains", ptr ? ptr : "");
	PREF_SavePrefFile();
	FREEIF(ptr);

FAIL:
	FREEIF(list);
	FREEIF(domainlist);
	
	return PR_FALSE;
}


int
MSG_DisplayHTMLDomainsDialog(MWContext* context)
{
	static XPDialogInfo dialogInfo = {
		XP_DIALOG_OK_BUTTON | XP_DIALOG_CANCEL_BUTTON,
		HTMLDomainsDialogDone,
		600,
		440
	};
	int status = 0;
	char* domainlist;
	char* list = NULL;
	char* tmp;
	XPDialogStrings* strings;
	int i;
	int num = 0;
	char** array = NULL;

	PREF_CopyCharPref("mail.htmldomains", &domainlist);

	status = msg_generate_domains_list(domainlist, &num, &array);
	if (status < 0) goto FAIL;

	for (i=0 ; i<num ; i++) {
		tmp = PR_smprintf("<option value=%d>%s\n", i, array[i]);
		if (!tmp) {
			status = MK_OUT_OF_MEMORY;
			goto FAIL;
		}
		StrAllocCat(list, tmp);
		XP_FREE(tmp);
	}

	strings = XP_GetDialogStrings(MK_MSG_HTML_DOMAINS_DIALOG);
	if (!strings) {
		status = MK_OUT_OF_MEMORY;
		goto FAIL;
	}
	XP_CopyDialogString(strings, 0, list ? list : "");
	XP_MakeHTMLDialog(context, &dialogInfo, MK_MSG_HTML_DOMAINS_DIALOG_TITLE,
					  strings, NULL,PR_FALSE);


FAIL:
	FREEIF(array);
	FREEIF(domainlist);
	FREEIF(list);
	return status;
}

/* 
 * Does in-place modification of input param to conform with son-of-1036 rules
 *
 * A newsgroup component is one portion of the newsgroup name, separated by
 * dots. So mcom.dev.fouroh has three newsgroup components. This function is
 * only concerned with one component because that's what we need for virtual
 * newsgroups.
 */
void msg_MakeLegalNewsgroupComponent (char *name)
{
	int i = 0;
	char ch;

	while ((ch = name[i]) != '\0')
	{
		/* legal chars are 0-9,a-z, and +,-,_ */

		if (!(ch >= '0' && ch <= '9'))
			if (!(ch >= 'a' && ch <= 'z'))
				if (!(ch == '+' || ch == '-' || ch == '_'))
				{
					/* ch is illegal. We can lowercase an uppercase letter
					 * but everything else goes to some legal char, like '_'
					 */
					if (ch >= 'A' && ch <= 'Z')
						name[i] += 32;
					else
						name[i] = '_';
				}
		i++;

		/* a newsgroup component is limited to 14 chars */
		if (i > 13)
		{
			name[i] = '\0';
			break;
		}
	}
}

void MSG_SetPercentProgress(MWContext *context, int32 numerator, int32 denominator)
{
	XP_ASSERT(numerator <= denominator && numerator >= 0 && denominator > 0);
	if (numerator > denominator || numerator < 0 || denominator < 0)
	{
		FE_SetProgressBarPercent(context, -1);
	}
	else if (denominator > 0)
	{
		int32 percent;
		if (denominator > 100L)
			percent = (numerator / (denominator / 100L));
		else
			percent = (100L * numerator) / denominator;
		FE_SetProgressBarPercent (context, percent);
	}
}

const char* MSG_FormatDateFromContext(MWContext *context, time_t date) 
{
  /* fix i18n.  Well, maybe.  Isn't strftime() supposed to be i18n? */
  /* ftong- Well.... strftime() in Mac and Window is not really i18n 		*/
  /* We need to use XP_StrfTime instead of strftime 						*/
  static char result[40];	/* 30 probably not enough */
  time_t now = time ((time_t *) 0);

  int32 offset = XP_LocalZoneOffset() * 60L; /* Number of seconds between
											 local and GMT. */

  int32 secsperday = 24L * 60L * 60L;

  int32 nowday = (now + offset) / secsperday;
  int32 day = (date + offset) / secsperday;

  if (day == nowday) {
	XP_StrfTime(context, result, sizeof(result), XP_TIME_FORMAT,
				localtime(&date));
  } else if (day < nowday && day > nowday - 7) {
	XP_StrfTime(context, result, sizeof(result), XP_WEEKDAY_TIME_FORMAT,
				localtime(&date));
  } else {
#if defined (XP_WIN)
  if (date < 0 || date > 0x7FFFFFFF)
	date = 0x7FFFFFFF;
#endif
  XP_StrfTime(context, result, sizeof(result), XP_DATE_TIME_FORMAT,
				localtime(&date));
  }

  return result;
}

