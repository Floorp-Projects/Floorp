
/* pngmem.c - stub functions for memory allocation

   libpng 1.0 beta 3 - version 0.89
   For conditions of distribution and use, see copyright notice in png.h
   Copyright (c) 1995, 1996 Guy Eric Schalnat, Group 42, Inc.
   May 25, 1996

   This file provides a location for all memory allocation.  Users which
   need special memory handling are expected to modify the code in this file
   to meet their needs.  See the instructions at each function. */

#define PNG_INTERNAL
#include "png.h"

/* Borland DOS special memory handler */
#if defined(__TURBOC__) && !defined(_Windows) && !defined(__FLAT__)
/* if you change this, be sure to change the one in png.h also */

/* Allocate memory for a png_struct.  The malloc and memset can be replaced
 * by a single call to calloc() if this is thought to improve performance.
 */
png_voidp
png_create_struct(uInt type)
{
   png_size_t type;
   png_voidp struct_ptr;

   if (type == PNG_STRUCT_INFO)
     size = sizeof(png_info);
   else if (type == PNG_STRUCT_PNG)
     size = sizeof(png_struct);
   else
     return (png_voidp)NULL;

   if ((struct_ptr = (png_voidp)farmalloc(size)) != NULL)
   {
      png_memset(struct_ptr, 0, size);
   }

   return (struct_ptr);
}


/* Free memory allocated by a png_create_struct() call */
void
png_destroy_struct(png_voidp struct_ptr)
{
   if (struct_ptr)
      farfree (struct_ptr);
}

/* Allocate memory.  For reasonable files, size should never exceed
   64K.  However, zlib may allocate more then 64K if you don't tell
   it not to.  See zconf.h and png.h for more information. zlib does
   need to allocate exactly 64K, so whatever you call here must
   have the ability to do that. */

/* Borland seems to have a problem in DOS mode for exactly 64K.
   It gives you a segment with an offset of 8 (perhaps to store it's
   memory stuff).  zlib doesn't like this at all, so we have to
   detect and deal with it.  This code should not be needed in
   Windows or OS/2 modes, and only in 16 bit mode.  This code has
   been updated by Alexander Lehmann for version 0.89 to waste less
   memory.
*/

png_voidp
png_large_malloc(png_structp png_ptr, png_uint_32 size)
{
   png_voidp ret;
   if (!png_ptr || !size)
      return ((voidp)NULL);

#ifdef PNG_MAX_MALLOC_64K
   if (size > (png_uint_32)65536L)
      png_error(png_ptr, "Cannot Allocate > 64K");
#endif

   if (size == (png_uint_32)(65536L))
   {
      if (!png_ptr->offset_table)
      {
         /* try to see if we need to do any of this fancy stuff */
         ret = farmalloc(size);
         if (!ret || ((long)ret & 0xffff))
         {
            int num_blocks;
            png_uint_32 total_size;
            png_bytep table;
            int i;
            png_byte huge * hptr;

            if (ret)
               farfree(ret);
            ret = NULL;

            num_blocks = (int)(1 << (png_ptr->zlib_window_bits - 14));
            if (num_blocks < 1)
               num_blocks = 1;
            if (png_ptr->zlib_mem_level >= 7)
               num_blocks += (int)(1 << (png_ptr->zlib_mem_level - 7));
            else
               num_blocks++;

            total_size = ((png_uint_32)65536L) * (png_uint_32)num_blocks+16;

            table = farmalloc(total_size);

            if (!table)
            {
               png_error(png_ptr, "Out of Memory");
            }

            if ((long)table & 0xfff0)
            {
               png_error(png_ptr, "Farmalloc didn't return normalized pointer");
            }

            png_ptr->offset_table = table;
            png_ptr->offset_table_ptr = farmalloc(
               num_blocks * sizeof (png_bytep));

            if (!png_ptr->offset_table_ptr)
            {
               png_error(png_ptr, "Out of memory");
            }

            hptr = (png_byte huge *)table;
            if ((long)hptr & 0xf)
            {
               hptr = (png_byte huge *)((long)(hptr) & 0xfffffff0L);
               hptr += 16L;
            }
            for (i = 0; i < num_blocks; i++)
            {
               png_ptr->offset_table_ptr[i] = (png_bytep)hptr;
               hptr += 65536L;
            }

            png_ptr->offset_table_number = num_blocks;
            png_ptr->offset_table_count = 0;
            png_ptr->offset_table_count_free = 0;
         }
      }

      if (png_ptr->offset_table_count >= png_ptr->offset_table_number)
         png_error(png_ptr, "Out of Memory");

      ret = png_ptr->offset_table_ptr[png_ptr->offset_table_count++];
   }
   else
      ret = farmalloc(size);

   if (ret == NULL)
   {
      png_error(png_ptr, "Out of Memory");
   }

   return ret;
}

/* free a pointer allocated by png_large_malloc().  In the default
  configuration, png_ptr is not used, but is passed in case it
  is needed.  If ptr is NULL, return without taking any action. */
void
png_large_free(png_structp png_ptr, png_voidp ptr)
{
   if (!png_ptr)
      return;

   if (ptr != NULL)
   {
      if (png_ptr->offset_table)
      {
         int i;

         for (i = 0; i < png_ptr->offset_table_count; i++)
         {
            if (ptr == png_ptr->offset_table_ptr[i])
            {
               ptr = 0;
               png_ptr->offset_table_count_free++;
               break;
            }
         }
         if (png_ptr->offset_table_count_free == png_ptr->offset_table_count)
         {
            farfree(png_ptr->offset_table);
            farfree(png_ptr->offset_table_ptr);
            png_ptr->offset_table = 0;
            png_ptr->offset_table_ptr = 0;
         }
      }

      if (ptr)
         farfree(ptr);
   }
}

#else /* Not the Borland DOS special memory handler */

/* Allocate memory for a png_struct or a png_info.  The malloc and
 * memset can be replaced by a single call to calloc() if this is thought
 * to improve performance noticably.
 */
png_voidp
png_create_struct(uInt type)
{
   size_t size;
   png_voidp struct_ptr;

   if (type == PNG_STRUCT_INFO)
     size = sizeof(png_info);
   else if (type == PNG_STRUCT_PNG)
     size = sizeof(png_struct);
   else
     return (png_voidp)NULL;

#if defined(__TURBOC__) && !defined(__FLAT__)
   if ((struct_ptr = (png_voidp)farmalloc(size)) != NULL)
#else
# if defined(_MSC_VER) && defined(MAXSEG_64K)
   if ((struct_ptr = (png_voidp)halloc(size,1)) != NULL)
# else
   if ((struct_ptr = (png_voidp)malloc(size)) != NULL)
# endif
#endif
   {
      png_memset(struct_ptr, 0, size);
   }

   return (struct_ptr);
}


/* Free memory allocated by a png_create_struct() call */
void
png_destroy_struct(png_voidp struct_ptr)
{
   if (struct_ptr)
#if defined(__TURBOC__) && !defined(__FLAT__)
      farfree(struct_ptr);
#else
# if defined(_MSC_VER) && defined(MAXSEG_64K)
      hfree(struct_ptr);
# else
      free(struct_ptr);
# endif
#endif
}


/* Allocate memory.  For reasonable files, size should never exceed
   64K.  However, zlib may allocate more then 64K if you don't tell
   it not to.  See zconf.h and png.h for more information. zlib does
   need to allocate exactly 64K, so whatever you call here must
   have the ability to do that. */

png_voidp
png_large_malloc(png_structp png_ptr, png_uint_32 size)
{
   png_voidp ret;
   if (!png_ptr || !size)
      return ((voidp)0);

#ifdef PNG_MAX_MALLOC_64K
   if (size > (png_uint_32)65536L)
      png_error(png_ptr, "Cannot Allocate > 64K");
#endif

#if defined(__TURBOC__) && !defined(__FLAT__)
   ret = farmalloc(size);
#else
# if defined(_MSC_VER) && defined(MAXSEG_64K)
   ret = halloc(size, 1);
# else
   ret = malloc(size);
# endif
#endif

   if (ret == NULL)
   {
      png_error(png_ptr, "Out of Memory");
   }

   return ret;
}

/* free a pointer allocated by png_large_malloc().  In the default
  configuration, png_ptr is not used, but is passed in case it
  is needed.  If ptr is NULL, return without taking any action. */
void
png_large_free(png_structp png_ptr, png_voidp ptr)
{
   if (!png_ptr)
      return;

   if (ptr != NULL)
   {
#if defined(__TURBOC__) && !defined(__FLAT__)
      farfree(ptr);
#else
# if defined(_MSC_VER) && defined(MAXSEG_64K)
      hfree(ptr);
# else
      free(ptr);
# endif
#endif
   }
}

#endif /* Not Borland DOS special memory handler */

/* Allocate memory.  This is called for smallish blocks only  It
   should not get anywhere near 64K.  On segmented machines, this
   must come from the local heap (for zlib).  Currently, zlib is
   the only one that uses this, so you should only get one call
   to this, and that a small block. */
void *
png_malloc(png_structp png_ptr, png_uint_32 size)
{
   void *ret;

   if (!png_ptr || !size)
   {
      return ((void *)0);
   }

#ifdef PNG_MAX_MALLOC_64K
   if (size > (png_uint_32)65536L)
      png_error(png_ptr, "Cannot Allocate > 64K");
#endif


   ret = malloc((png_size_t)size);

   if (!ret)
   {
      png_error(png_ptr, "Out of Memory");
   }

   return ret;
}

/* Reallocate memory.  This will not get near 64K on a
   even marginally reasonable file.  This is not used in
   the current version of the library. */
void *
png_realloc(png_structp png_ptr, void * ptr, png_uint_32 size,
   png_uint_32 old_size)
{
   void *ret;

   if (!png_ptr || !old_size || !ptr || !size)
      return ((void *)0);

#ifdef PNG_MAX_MALLOC_64K
   if (size > (png_uint_32)65536L)
      png_error(png_ptr, "Cannot Allocate > 64K");
#endif

   ret = realloc(ptr, (png_size_t)size);

   if (!ret)
   {
      png_error(png_ptr, "Out of Memory 7");
   }

   return ret;
}

/* free a pointer allocated by png_malloc().  In the default
  configuration, png_ptr is not used, but is passed incase it
  is needed.  If ptr is NULL, return without taking any action. */
void
png_free(png_structp png_ptr, void * ptr)
{
   if (!png_ptr)
      return;

   if (ptr != (void *)0)
      free(ptr);
}

