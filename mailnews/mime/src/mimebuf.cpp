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
/* 
 * mimebuf.c -  libmsg like buffer handling routines for libmime
 */
#include "prmem.h"
#include "plstr.h"
#include "prlog.h"
#include "nsCRT.h"
#include "msgCore.h"

extern "C" int MK_OUT_OF_MEMORY;

extern "C" int
mime_GrowBuffer (PRUint32 desired_size, PRUint32 element_size, PRUint32 quantum,
				char **buffer, PRInt32 *size)
{
  if (*size <= desired_size)
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
		  return MK_OUT_OF_MEMORY;
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

  if (desired_buffer_size >= (*buffer_sizeP))
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
      nsCRT::memcpy ((*bufferP) + (*buffer_fpP), net_buffer, size);
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
							PRInt32 (*per_line_fn) (char *line,
												  PRUint32 line_length,
												  void *closure),
							void *closure)
{
  /* Convert the line terminator to the native form.
   */
  char* newline;

  PR_ASSERT(buf && length > 0);
  if (!buf || length <= 0) return -1;
  newline = buf + length;
  PR_ASSERT(newline[-1] == CR || newline[-1] == LF);
  if (newline[-1] != CR && newline[-1] != LF) return -1;

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

extern "C" int
mime_LineBuffer (const char *net_buffer, PRInt32 net_buffer_size,
				char **bufferP, PRInt32 *buffer_sizeP, PRUint32 *buffer_fpP,
				PRBool convert_newlines_p,
				PRInt32 (*per_line_fn) (char *line, PRUint32 line_length,
									  void *closure),
				void *closure)
{
  int status = 0;
  if (*buffer_fpP > 0 && *bufferP && (*bufferP)[*buffer_fpP - 1] == CR &&
	  net_buffer_size > 0 && net_buffer[0] != LF) {
	/* The last buffer ended with a CR.  The new buffer does not start
	   with a LF.  This old buffer should be shipped out and discarded. */
	PR_ASSERT(*buffer_sizeP > *buffer_fpP);
	if (*buffer_sizeP <= *buffer_fpP) return -1;
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
		PRUint32 desired_size = (end - net_buffer) + (*buffer_fpP) + 1;

		if (desired_size >= (*buffer_sizeP))
		  {
			status = mime_GrowBuffer (desired_size, sizeof(char), 1024,
									 bufferP, buffer_sizeP);
			if (status < 0) return status;
		  }
		nsCRT::memcpy ((*bufferP) + (*buffer_fpP), net_buffer, (end - net_buffer));
		(*buffer_fpP) += (end - net_buffer);
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
	  if (status < 0) return status;

	  net_buffer_size -= (newline - net_buffer);
	  net_buffer = newline;
	  (*buffer_fpP) = 0;
	}
  return 0;
}

/*	Very similar to strdup except it free's too
 */
extern "C" char * 
mime_SACopy (char **destination, const char *source)
{
  if(*destination)
  {
    PR_Free(*destination);
    *destination = 0;
  }
  if (! source)
  {
    *destination = NULL;
  }
  else 
  {
    *destination = (char *) PR_Malloc (PL_strlen(source) + 1);
    if (*destination == NULL) 
      return(NULL);
    
    PL_strcpy (*destination, source);
  }
  return *destination;
}

/*  Again like strdup but it concatinates and free's and uses Realloc
*/
extern "C"  char *
mime_SACat (char **destination, const char *source)
{
  if (source && *source)
  {
    if (*destination)
    {
      int length = PL_strlen (*destination);
      *destination = (char *) PR_Realloc (*destination, length + PL_strlen(source) + 1);
      if (*destination == NULL)
        return(NULL);
      
      PL_strcpy (*destination + length, source);
    }
    else
    {
      *destination = (char *) PR_Malloc (PL_strlen(source) + 1);
      if (*destination == NULL)
        return(NULL);
      
      PL_strcpy (*destination, source);
    }
  }
  return *destination;
}
