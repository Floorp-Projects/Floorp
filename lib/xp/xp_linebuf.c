/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */


/* xp_linebuf.c -- general line-buffering and de-buffering of text streams.
   Used by libmime, llibmsg, and others.

   This stuff used to be in libmsg/msgutils.c, but it's really not
   mail-specific.  And I had to move it to get libmime to work in the
   absence of libmsg, so in the meantime, I changed the prefix from MSG_
   to XP_.
 */

#include "xp_mcom.h"
#include "xp_linebuf.h"

extern int MK_OUT_OF_MEMORY;


/* Amazingly enough, these seem only to be defined in fe_proto.h, which is
   totally the wrong thing to be including here.
 */
#define CR '\015'
#define LF '\012'
#define CRLF "\015\012"

#ifdef XP_MAC
# define LINEBREAK          "\012"
# define LINEBREAK_LEN      1
#else
# if defined(XP_WIN) || defined(XP_OS2)
#  define LINEBREAK         "\015\012"
#  define LINEBREAK_LEN     2
# else
#  if defined(XP_UNIX) || defined(XP_BEOS)
#   define LINEBREAK        "\012"
#   define LINEBREAK_LEN    1
#  endif /* XP_UNIX */
# endif /* XP_WIN */
#endif /* XP_MAC */



/* Buffer management.
   Why do I feel like I've written this a hundred times before?
 */
int
XP_GrowBuffer (uint32 desired_size, uint32 element_size, uint32 quantum,
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


#if 0 /* #### what is this??  and what is it doing here?? */
extern XP_Bool NET_POP3TooEarlyForEnd(int32 len);
#endif

/* Take the given buffer, tweak the newlines at the end if necessary, and
   send it off to the given routine.  We are guaranteed that the given
   buffer has allocated space for at least one more character at the end. */
static int
xp_convert_and_send_buffer(char* buf, uint32 length,
                           XP_Bool convert_newlines_p,
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

  /* update count of bytes parsed adding/removing CR or LF */
#if 0 /* #### what is this??  and what is it doing here?? */
  NET_POP3TooEarlyForEnd(length);
#endif

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
XP_LineBuffer (const char *net_buffer, int32 net_buffer_size,
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
    status = xp_convert_and_send_buffer(*bufferP, *buffer_fpP,
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
            status = XP_GrowBuffer (desired_size, sizeof(char), 1024,
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

      status = xp_convert_and_send_buffer(*bufferP, *buffer_fpP,
                                          convert_newlines_p,
                                          per_line_fn, closure);
      if (status < 0) return status;

      net_buffer_size -= (newline - net_buffer);
      net_buffer = newline;
      (*buffer_fpP) = 0;
    }
  return 0;
}


/* The opposite of xp_LineBuffer(): takes small buffers and packs them
   up into bigger buffers before passing them along.

   Pass in a desired_buffer_size 0 to tell it to flush (for example, in
   in the very last call to this function.)
 */
int
XP_ReBuffer (const char *net_buffer, int32 net_buffer_size,
             uint32 desired_buffer_size,
             char **bufferP, uint32 *buffer_sizeP, uint32 *buffer_fpP,
             int32 (*per_buffer_fn) (char *buffer, uint32 buffer_size,
                                     void *closure),
             void *closure)
{
  int status = 0;

  if (desired_buffer_size >= (*buffer_sizeP))
    {
      status = XP_GrowBuffer (desired_buffer_size, sizeof(char), 1024,
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


