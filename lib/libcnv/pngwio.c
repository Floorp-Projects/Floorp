
/* pngwio.c - functions for data output

   libpng 1.0 beta 3 - version 0.89
   For conditions of distribution and use, see copyright notice in png.h
   Copyright (c) 1995, 1996 Guy Eric Schalnat, Group 42, Inc.
   May 25, 1996

   This file provides a location for all output.  Users which need
   special handling are expected to write functions which have the same
   arguments as these, and perform similar functions, but possibly use
   different output methods.  Note that you shouldn't change these
   functions, but rather write replacement functions and then change
   them at run time with png_set_write_fn(...) */

#define PNG_INTERNAL
#include "png.h"

/* Write the data to whatever output you are using.  The default routine
   writes to a file pointer.  Note that this routine sometimes gets called
   with very small lengths, so you should implement some kind of simple
   buffering if you are using unbuffered writes.  This should never be asked
   to write more then 64K on a 16 bit machine.  The cast to png_size_t is
   there to quiet warnings of certain compilers. */

void
png_write_data(png_structp png_ptr, png_bytep data, png_uint_32 length)
{
   if (png_ptr->write_data_fn)
      (*(png_ptr->write_data_fn))(png_ptr, data, length);
   else
      png_error(png_ptr, "Call to NULL write function");
}

/* This is the function which does the actual writing of data.  If you are
   not writing to a standard C stream, you should create a replacement
   write_data function and use it at run time with png_set_write_fn(), rather
   than changing the library. */
#ifndef USE_FAR_KEYWORD
static void
png_default_write_data(png_structp png_ptr, png_bytep data, png_uint_32 length)
{
   png_uint_32 check;

   check = fwrite(data, 1, (png_size_t)length, (FILE *)(png_ptr->io_ptr));
   if (check != length)
   {
      png_error(png_ptr, "Write Error");
   }
}
#else
/* this is the model-independent version. Since the standard I/O library
   can't handle far buffers in the medium and small models, we have to copy
   the data.
*/

#define NEAR_BUF_SIZE 1024
#define MIN(a,b) (a <= b ? a : b)

#ifdef _MSC_VER
/* for FP_OFF */
#include <dos.h>
#endif

static void
png_default_write_data(png_structp png_ptr, png_bytep data, png_uint_32 length)
{
   png_uint_32 check;
   png_byte *n_data;

   /* Check if data really is near. If so, use usual code. */
#ifdef _MSC_VER
   /* do it this way just to quiet warning */
   FP_OFF(n_data) = FP_OFF(data);
   if (FP_SEG(n_data) == FP_SEG(data))
#else
   /* this works in MSC also but with lost segment warning */
   n_data = (png_byte *)data;
   if ((png_bytep)n_data == data)
#endif
   {
      check = fwrite(n_data, 1, (png_size_t)length, (FILE *)(png_ptr->io_ptr));
   }
   else
   {
      png_byte buf[NEAR_BUF_SIZE];
      png_size_t written, remaining, err;
      check = 0;
      remaining = (png_size_t)length;
      do
      {
         written = MIN(NEAR_BUF_SIZE, remaining);
         png_memcpy(buf, data, written); /* copy far buffer to near buffer */
         err = fwrite(buf, 1, written, (FILE *)(png_ptr->io_ptr));
         if (err != written)
            break;
         else
            check += err;
         data += written;
         remaining -= written;
      }
      while (remaining != 0);
   }
   if (check != length)
   {
      png_error(png_ptr, "Write Error");
   }
}

#endif

/* This function is called to output any data pending writing (normally
   to disk).  After png_flush is called, there should be no data pending
   writing in any buffers. */
#if defined(PNG_WRITE_FLUSH_SUPPORTED)
void
png_flush(png_structp png_ptr)
{
   if (png_ptr->output_flush_fn)
      (*(png_ptr->output_flush_fn))(png_ptr);
}

static void
png_default_flush(png_structp png_ptr)
{
   if ((FILE *)(png_ptr->io_ptr))
      fflush((FILE *)(png_ptr->io_ptr));
}
#endif

/* This function allows the application to supply new output functions for
   libpng if standard C streams aren't being used.

   This function takes as its arguments:
   png_ptr       - pointer to a png output data structure
   io_ptr        - pointer to user supplied structure containing info about
                   the output functions.  May be NULL.
   write_data_fn - pointer to a new output function which takes as its
                   arguments a pointer to a png_struct, a pointer to
                   data to be written, and a 32-bit unsigned int which is
                   the number of bytes to be written.  The new write
                   function should call png_error(png_ptr, "Error msg")
                   to exit and output any fatal error messages.
   flush_data_fn - pointer to a new flush function which takes as its
                   arguments a pointer to a png_struct.  After a call to
                   the flush function, there should be no data in any buffers
                   or pending transmission.  If the output method doesn't do
                   any buffering of ouput, a function prototype must still be
                   supplied although it doesn't have to do anything.  If
                   PNG_WRITE_FLUSH_SUPPORTED is not defined at libpng compile
                   time, output_flush_fn will be ignored, although it must be
                   supplied for compatibility. */
void
png_set_write_fn(png_structp png_ptr, png_voidp io_ptr,
   png_rw_ptr write_data_fn, png_flush_ptr output_flush_fn)
{
   png_ptr->io_ptr = io_ptr;

   if (write_data_fn)
      png_ptr->write_data_fn = write_data_fn;
   else
      png_ptr->write_data_fn = png_default_write_data;

#if defined(PNG_WRITE_FLUSH_SUPPORTED)
   if (output_flush_fn)
      png_ptr->output_flush_fn = output_flush_fn;
   else
      png_ptr->output_flush_fn = png_default_flush;
#endif /* PNG_WRITE_FLUSH_SUPPORTED */

   /* It is an error to read while writing a png file */
   png_ptr->read_data_fn = NULL;
}

