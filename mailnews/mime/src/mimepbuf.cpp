/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsCOMPtr.h"
#include "mimepbuf.h"
#include "mimemoz2.h"
#include "prmem.h"
#include "prio.h"
#include "plstr.h"
#include "nsCRT.h"
#include "nsMimeStringResources.h"
#include "nsFileSpec.h"
#include "nsFileStream.h"

//
// External Defines...
//
extern nsFileSpec *nsMsgCreateTempFileSpec(const char *tFileName);

/* See mimepbuf.h for a description of the mission of this file.

   Implementation:

     When asked to buffer an object, we first try to malloc() a buffer to
	 hold the upcoming part.  First we try to allocate a 50k buffer, and
	 then back off by 5k until we are able to complete the allocation,
	 or are unable to allocate anything.

	 As data is handed to us, we store it in the memory buffer, until the
	 size of the memory buffer is exceeded (including the case where no
	 memory buffer was able to be allocated at all.)

	 Once we've filled the memory buffer, we open a temp file on disk.
	 Anything that is currently in the memory buffer is then flushed out
	 to the disk file (and the memory buffer is discarded.)  Subsequent
	 data that is passed in is appended to the file.

	 Thus only one of the memory buffer or the disk buffer ever exist at
	 the same time; and small parts tend to live completely in memory
	 while large parts tend to live on disk.

	 When we are asked to read the data back out of the buffer, we call
	 the provided read-function with either: the contents of the memory
	 buffer; or blocks read from the disk file.
 */

#define TARGET_MEMORY_BUFFER_SIZE    (1024 * 50)  /* try for 50k mem buffer */
#define TARGET_MEMORY_BUFFER_QUANTUM (1024 * 5)   /* decrease in steps of 5k */
#define DISK_BUFFER_SIZE			 (1024 * 10)  /* read disk in 10k chunks */


struct MimePartBufferData
{
  char        *part_buffer;				  /* Buffer used for part-lookahead. */
  PRInt32     part_buffer_fp;				/* Active length. */
  PRInt32     part_buffer_size;			/* How big it is. */

	nsFileSpec          *file_buffer_spec;		/* The nsFileSpec of a temp file used when we
								                               run out of room in the head_buffer. */
	nsInputFileStream   *input_file_stream;		/* A stream to it. */
	nsOutputFileStream  *output_file_stream;	/* A stream to it. */
};

MimePartBufferData *
MimePartBufferCreate (void)
{
  MimePartBufferData *data = PR_NEW(MimePartBufferData);
  if (!data) return 0;
  memset(data, 0, sizeof(*data));
  return data;
}


void
MimePartBufferClose (MimePartBufferData *data)
{
  NS_ASSERTION(data, "MimePartBufferClose: no data");
  if (!data) return;

	if (data->input_file_stream) 
  {
		data->input_file_stream->close();
    delete data->input_file_stream;
    data->input_file_stream = nsnull;
	}

  if (data->output_file_stream) 
  {
		data->output_file_stream->close();
    delete data->output_file_stream;
    data->output_file_stream = nsnull;
	}
}


void
MimePartBufferReset (MimePartBufferData *data)
{
  NS_ASSERTION(data, "MimePartBufferReset: no data");
  if (!data) return;

  PR_FREEIF(data->part_buffer);
  data->part_buffer_fp = 0;

	if (data->input_file_stream) 
  {
		data->input_file_stream->close();
    delete data->input_file_stream;
    data->input_file_stream = nsnull;
	}

  if (data->output_file_stream) 
  {
		data->output_file_stream->close();
    delete data->output_file_stream;
    data->output_file_stream = nsnull;
	}

	if (data->file_buffer_spec) 
  {
    data->file_buffer_spec->Delete(PR_FALSE);
    delete data->file_buffer_spec;
    data->file_buffer_spec = nsnull;
	}
}


void
MimePartBufferDestroy (MimePartBufferData *data)
{
  NS_ASSERTION(data, "MimePartBufferDestroy: no data");
  if (!data) return;
  MimePartBufferReset (data);
  PR_Free(data);
}


int
MimePartBufferWrite (MimePartBufferData *data,
					 const char *buf, PRInt32 size)
{
  int status = 0;

  NS_ASSERTION(data && buf && size > 0, "MimePartBufferWrite: Bad param");
  if (!data || !buf || size <= 0)
	return -1;

  /* If we don't yet have a buffer (either memory or file) try and make a
	 memory buffer.
   */
  if (!data->part_buffer &&
	    !data->file_buffer_spec)
	{
	  int target_size = TARGET_MEMORY_BUFFER_SIZE;
	  while (target_size > 0)
		{
		  data->part_buffer = (char *) PR_MALLOC(target_size);
		  if (data->part_buffer) break;					/* got it! */
		  target_size -= TARGET_MEMORY_BUFFER_QUANTUM;	/* decrease it and try
														   again */
		}

	  if (data->part_buffer)
		data->part_buffer_size = target_size;
	  else
		data->part_buffer_size = 0;

	  data->part_buffer_fp = 0;
	}

  /* Ok, if at this point we still don't have either kind of buffer, try and
	 make a file buffer. */
  if (!data->part_buffer && !data->file_buffer_spec)
	{
    data->file_buffer_spec = nsMsgCreateTempFileSpec("nsma");
		if (!data->file_buffer_spec) 
      return MIME_OUT_OF_MEMORY;

    data->output_file_stream = new nsOutputFileStream(*(data->file_buffer_spec), PR_WRONLY | PR_CREATE_FILE, 00600);
		if (!data->output_file_stream) 
    {
			return MIME_UNABLE_TO_OPEN_TMP_FILE;
		}
  }

  NS_ASSERTION(data->part_buffer || data->output_file_stream, "no part_buffer or file_stream");

  /* If this buf will fit in the memory buffer, put it there.
   */
  if (data->part_buffer &&
	  data->part_buffer_fp + size < data->part_buffer_size)
	{
	  nsCRT::memcpy(data->part_buffer + data->part_buffer_fp,
				buf, size);
	  data->part_buffer_fp += size;
	}

  /* Otherwise it won't fit; write it to the file instead. */
  else
	{
	  /* If the file isn't open yet, open it, and dump the memory buffer
		 to it. */
	  if (!data->output_file_stream)
		{
		  if (!data->file_buffer_spec)
  			data->file_buffer_spec = nsMsgCreateTempFileSpec("nsma");
		  if (!data->file_buffer_spec) 
        return MIME_OUT_OF_MEMORY;

      data->output_file_stream = new nsOutputFileStream(*(data->file_buffer_spec), PR_WRONLY | PR_CREATE_FILE, 00600);
		  if (!data->output_file_stream) 
      {
			  return MIME_UNABLE_TO_OPEN_TMP_FILE;
		  }

		  if (data->part_buffer && data->part_buffer_fp)
			{
			  status = data->output_file_stream->write(data->part_buffer,
                                      					 data->part_buffer_fp);
			  if (status < data->part_buffer_fp) 
          return MIME_OUT_OF_MEMORY;
			}

		  PR_FREEIF(data->part_buffer);
		  data->part_buffer_fp = 0;
		  data->part_buffer_size = 0;
		}

	  /* Dump this buf to the file. */
	  status = data->output_file_stream->write (buf, size);
	  if (status < size) 
      return MIME_OUT_OF_MEMORY;
	}

  return 0;
}


int
MimePartBufferRead (MimePartBufferData *data,
					nsresult (*read_fn) (char *buf, PRInt32 size, void *closure),
					void *closure)
{
  int status = 0;
  NS_ASSERTION(data, "no data");
  if (!data) return -1;

  if (data->part_buffer)
	{
	  // Read it out of memory.
	  status = read_fn(data->part_buffer, data->part_buffer_fp, closure);
	}
  else if (data->file_buffer_spec)
	{
	  /* Read it off disk.
	   */
	  char *buf;
	  PRInt32 buf_size = DISK_BUFFER_SIZE;

	  NS_ASSERTION(data->part_buffer_size == 0 && data->part_buffer_fp == 0, "buffer size is not null");
	  NS_ASSERTION(data->file_buffer_spec, "no file buffer name");
	  if (!data->file_buffer_spec) 
      return -1;

	  buf = (char *) PR_MALLOC(buf_size);
	  if (!buf) 
      return MIME_OUT_OF_MEMORY;

    // First, close the output file to open the input file!
    if (data->output_file_stream)
  		data->output_file_stream->close();

		data->input_file_stream = new nsInputFileStream(*(data->file_buffer_spec));
		if (!data->input_file_stream) 
    {
			PR_Free(buf);
			return MIME_UNABLE_TO_OPEN_TMP_FILE;
		}

    while(1)
		{
		  PRInt32 rstatus = data->input_file_stream->read(buf, buf_size - 1);
		  if (rstatus <= 0)
			{
			  status = rstatus;
			  break;
			}
		  else
			{
			  /* It would be really nice to be able to yield here, and let
				 some user events and other input sources get processed.
				 Oh well. */

			  status = read_fn (buf, rstatus, closure);
			  if (status < 0) break;
			}
		}
	  PR_Free(buf);
	}

  return 0;
}
