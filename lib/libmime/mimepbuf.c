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

/* mimepbuf.c --- utilities for buffering a MIME part for later display.
   Created: Jamie Zawinski <jwz@netscape.com>, 24-Sep-96.
 */

#include "mimepbuf.h"
#include "xp_file.h"

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

extern int MK_UNABLE_TO_OPEN_TMP_FILE;

#define TARGET_MEMORY_BUFFER_SIZE    (1024 * 50)  /* try for 50k mem buffer */
#define TARGET_MEMORY_BUFFER_QUANTUM (1024 * 5)   /* decrease in steps of 5k */
#define DISK_BUFFER_SIZE			 (1024 * 10)  /* read disk in 10k chunks */


struct MimePartBufferData
{
  char *part_buffer;				/* Buffer used for part-lookahead. */
  int32 part_buffer_fp;				/* Active length. */
  int32 part_buffer_size;			/* How big it is. */

  char *file_buffer_name;			/* The name of a temp file used when we
									   run out of room in the part_buffer. */
  XP_File file_stream;				/* A stream to it. */
};

MimePartBufferData *
MimePartBufferCreate (void)
{
  MimePartBufferData *data = XP_NEW(MimePartBufferData);
  if (!data) return 0;
  XP_MEMSET(data, 0, sizeof(*data));
  return data;
}


void
MimePartBufferClose (MimePartBufferData *data)
{
  XP_ASSERT(data);
  if (!data) return;

  if (data->file_stream)
	{
	  XP_FileClose(data->file_stream);
	  data->file_stream = 0;
	}
}


void
MimePartBufferReset (MimePartBufferData *data)
{
  XP_ASSERT(data);
  if (!data) return;

  FREEIF(data->part_buffer);
  data->part_buffer_fp = 0;

  if (data->file_stream)
	{
	  XP_FileClose(data->file_stream);
	  data->file_stream = 0;
	}
  if (data->file_buffer_name)
	{
	  XP_FileRemove(data->file_buffer_name, xpTemporary);
	  XP_FREE(data->file_buffer_name);
	  data->file_buffer_name = 0;
	}
}


void
MimePartBufferDestroy (MimePartBufferData *data)
{
  XP_ASSERT(data);
  if (!data) return;
  MimePartBufferReset (data);
  XP_FREE(data);
}


int
MimePartBufferWrite (MimePartBufferData *data,
					 const char *buf, int32 size)
{
  int status = 0;

  XP_ASSERT(data && buf && size > 0);
  if (!data || !buf || size <= 0)
	return -1;

  /* If we don't yet have a buffer (either memory or file) try and make a
	 memory buffer.
   */
  if (!data->part_buffer &&
	  !data->file_buffer_name)
	{
	  int target_size = TARGET_MEMORY_BUFFER_SIZE;
	  while (target_size > 0)
		{
		  data->part_buffer = (char *) XP_ALLOC(target_size);
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
  if (!data->part_buffer &&
	  !data->file_buffer_name)
	{
	  data->file_buffer_name = WH_TempName(xpTemporary, "nsma");
	  if (!data->file_buffer_name) return MK_OUT_OF_MEMORY;

	  data->file_stream = XP_FileOpen(data->file_buffer_name, xpTemporary,
									  XP_FILE_WRITE_BIN);
	  if (!data->file_stream)
		return MK_UNABLE_TO_OPEN_TMP_FILE;
	}
  
  XP_ASSERT(data->part_buffer || data->file_stream);


  /* If this buf will fit in the memory buffer, put it there.
   */
  if (data->part_buffer &&
	  data->part_buffer_fp + size < data->part_buffer_size)
	{
	  XP_MEMCPY(data->part_buffer + data->part_buffer_fp,
				buf, size);
	  data->part_buffer_fp += size;
	}

  /* Otherwise it won't fit; write it to the file instead. */
  else
	{
	  /* If the file isn't open yet, open it, and dump the memory buffer
		 to it. */
	  if (!data->file_stream)
		{
		  if (!data->file_buffer_name)
			data->file_buffer_name =
			  WH_TempName (xpTemporary, "nsma");
		  if (!data->file_buffer_name) return MK_OUT_OF_MEMORY;

		  data->file_stream = XP_FileOpen(data->file_buffer_name, xpTemporary,
										  XP_FILE_WRITE_BIN);
		  if (!data->file_stream)
			return MK_UNABLE_TO_OPEN_TMP_FILE;

		  if (data->part_buffer && data->part_buffer_fp)
			{
			  status = XP_FileWrite (data->part_buffer,
									 data->part_buffer_fp,
									 data->file_stream);
			  if (status < 0) return status;
			}

		  FREEIF(data->part_buffer);
		  data->part_buffer_fp = 0;
		  data->part_buffer_size = 0;
		}

	  /* Dump this buf to the file. */
	  status = XP_FileWrite (buf, size, data->file_stream);
	  if (status < 0) return status;
	}

  return 0;
}


int
MimePartBufferRead (MimePartBufferData *data,
					int (*read_fn) (char *buf, int32 size, void *closure),
					void *closure)
{
  int status = 0;
  XP_ASSERT(data);
  if (!data) return -1;

  if (data->part_buffer)
	{
	  /* Read it out of memory.
	   */
	  XP_ASSERT(!data->file_buffer_name && !data->file_stream);

	  status = read_fn(data->part_buffer, data->part_buffer_fp, closure);
	}
  else if (data->file_buffer_name)
	{
	  /* Read it off disk.
	   */
	  char *buf;
	  int32 buf_size = DISK_BUFFER_SIZE;

	  XP_ASSERT(data->part_buffer_size == 0 && data->part_buffer_fp == 0);
	  XP_ASSERT(!data->file_stream);
	  XP_ASSERT(data->file_buffer_name);
	  if (!data->file_buffer_name) return -1;

	  buf = (char *) XP_ALLOC(buf_size);
	  if (!buf) return MK_OUT_OF_MEMORY;

	  if (data->file_stream)
		XP_FileClose(data->file_stream);
	  data->file_stream = XP_FileOpen(data->file_buffer_name, xpTemporary,
									  XP_FILE_READ_BIN);
	  if (!data->file_stream)
		{
		  XP_FREE(buf);
		  return MK_UNABLE_TO_OPEN_TMP_FILE;
		}

	  while(1)
		{
		  int32 rstatus = XP_FileRead(buf, buf_size - 1, data->file_stream);
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
	  XP_FREE(buf);
	}

  return 0;
}
