/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 VisualAge build.
 */
/* 
 * mimebuf.c -  libmsg like buffer handling routines for libmime
 */
#include "prmem.h"
#include "plstr.h"
#include "prlog.h"
#include "nsCRT.h"
#include "msgCore.h"
#include "nsMimeStringResources.h"

extern "C" int
mime_GrowBuffer (PRUint32 desired_size, PRUint32 element_size, PRUint32 quantum,
				char **buffer, PRInt32 *size)
{
  if ((PRUint32) *size <= desired_size)
	{
	  char *new_buf;
	  PRUint32 increment = desired_size - *size;
	  if (increment < quantum) /* always grow by a minimum of N bytes */
		increment = quantum;

	  new_buf = (*buffer
				 ? (char *) PR_Realloc (*buffer, (*size + increment)
										* (element_size / sizeof(char)))
				 : (char *) PR_MALLOC ((*size + increment)
									  * (element_size / sizeof(char))));
	  if (! new_buf)
		  return MIME_OUT_OF_MEMORY;
	  *buffer = new_buf;
	  *size += increment;
	}
  return 0;
}

/* The opposite of mime_LineBuffer(): takes small buffers and packs them
   up into bigger buffers before passing them along.

   Pass in a desired_buffer_size 0 to tell it to flush (for example, in
   in the very last call to this function.)
 */
extern "C" int
mime_ReBuffer (const char *net_buffer, PRInt32 net_buffer_size,
			  PRUint32 desired_buffer_size,
			  char **bufferP, PRInt32 *buffer_sizeP, PRUint32 *buffer_fpP,
			  PRInt32 (*per_buffer_fn) (char *buffer, PRUint32 buffer_size,
									  void *closure),
			  void *closure)
{
  int status = 0;

  if (desired_buffer_size >= (PRUint32) (*buffer_sizeP))
	{
	  status = mime_GrowBuffer (desired_buffer_size, sizeof(char), 1024,
							   bufferP, buffer_sizeP);
	  if (status < 0) return status;
	}

  do
	{
	  PRInt32 size = *buffer_sizeP - *buffer_fpP;
	  if (size > net_buffer_size)
		size = net_buffer_size;
	  if (size > 0)
		{
		  memcpy ((*bufferP) + (*buffer_fpP), net_buffer, size);
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

static int
convert_and_send_buffer(char* buf, int length, PRBool convert_newlines_p,
							PRInt32 (*PR_CALLBACK per_line_fn) (char *line,
												  PRUint32 line_length,
												  void *closure),
							void *closure)
{
  /* Convert the line terminator to the native form.
   */
  char* newline;

#if (MSG_LINEBREAK_LEN == 2)
  /***
  * This is a patch to support a mail DB corruption cause by earlier version that lead to a crash.
  * What happened is that the line terminator is CR+NULL+LF. Therefore, we first process a line
  * terminated by CR then a second line that contains only NULL+LF. We need to ignore this second
  * line. See bug http://bugzilla.mozilla.org/show_bug.cgi?id=61412 for more information.
  ***/
  if (length == 2 && buf[0] == 0x00 && buf[1] == nsCRT::LF)
    return 0;
#endif

  NS_ASSERTION(buf && length > 0, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");
  if (!buf || length <= 0) return -1;
  newline = buf + length;
  NS_ASSERTION(newline[-1] == nsCRT::CR || newline[-1] == nsCRT::LF, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");
  if (newline[-1] != nsCRT::CR && newline[-1] != nsCRT::LF) return -1;

  if (!convert_newlines_p)
	{
	}
#if (MSG_LINEBREAK_LEN == 1)
  else if ((newline - buf) >= 2 &&
		   newline[-2] == nsCRT::CR &&
		   newline[-1] == nsCRT::LF)
	{
	  /* CRLF -> CR or LF */
	  buf [length - 2] = MSG_LINEBREAK[0];
	  length--;
	}
  else if (newline > buf + 1 &&
		   newline[-1] != MSG_LINEBREAK[0])
	{
	  /* CR -> LF or LF -> CR */
	  buf [length - 1] = MSG_LINEBREAK[0];
	}
#else
  else if (((newline - buf) >= 2 && newline[-2] != nsCRT::CR) ||
		   ((newline - buf) >= 1 && newline[-1] != nsCRT::LF))
	{
	  /* LF -> CRLF or CR -> CRLF */
	  length++;
	  buf[length - 2] = MSG_LINEBREAK[0];
	  buf[length - 1] = MSG_LINEBREAK[1];
	}
#endif

  return (*per_line_fn)(buf, length, closure);
}

extern "C" int
mime_LineBuffer (const char *net_buffer, PRInt32 net_buffer_size,
				char **bufferP, PRInt32 *buffer_sizeP, PRUint32 *buffer_fpP,
				PRBool convert_newlines_p,
				PRInt32 (*PR_CALLBACK per_line_fn) (char *line, PRUint32 line_length,
									  void *closure),
				void *closure)
{
  int status = 0;
  if (*buffer_fpP > 0 && *bufferP && (*bufferP)[*buffer_fpP - 1] == nsCRT::CR &&
	  net_buffer_size > 0 && net_buffer[0] != nsCRT::LF) {
	/* The last buffer ended with a CR.  The new buffer does not start
	   with a LF.  This old buffer should be shipped out and discarded. */
	NS_ASSERTION((PRUint32) *buffer_sizeP > *buffer_fpP, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");
	if ((PRUint32) *buffer_sizeP <= *buffer_fpP) return -1;
	status = convert_and_send_buffer(*bufferP, *buffer_fpP,
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
		  if (*s == nsCRT::CR || *s == nsCRT::LF)
			{
			  newline = s;
			  if (newline[0] == nsCRT::CR)
				{
				  if (s == net_buffer_end - 1)
					{
					  /* CR at end - wait for the next character. */
					  newline = 0;
					  break;
					}
				  else if (newline[1] == nsCRT::LF)
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
		PRUint32 desired_size = (end - net_buffer) + (*buffer_fpP) + 1;

		if (desired_size >= (PRUint32) (*buffer_sizeP))
		  {
			status = mime_GrowBuffer (desired_size, sizeof(char), 1024,
									 bufferP, buffer_sizeP);
			if (status < 0) return status;
		  }
		memcpy ((*bufferP) + (*buffer_fpP), net_buffer, (end - net_buffer));
		(*buffer_fpP) += (end - net_buffer);
        (*bufferP)[*buffer_fpP] = '\0';
	  }

	  /* Now *bufferP contains either a complete line, or as complete
		 a line as we have read so far.

		 If we have a line, process it, and then remove it from `*bufferP'.
		 Then go around the loop again, until we drain the incoming data.
	   */
	  if (!newline)
		return 0;

	  status = convert_and_send_buffer(*bufferP, *buffer_fpP,
										   convert_newlines_p,
										   per_line_fn, closure);
	  if (status < 0) 
      return status;

	  net_buffer_size -= (newline - net_buffer);
	  net_buffer = newline;
	  (*buffer_fpP) = 0;
	}
  return 0;
}
