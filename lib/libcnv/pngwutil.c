
/* pngwutil.c - utilities to write a png file

   libpng 1.0 beta 3 - version 0.89
   For conditions of distribution and use, see copyright notice in png.h
   Copyright (c) 1995, 1996 Guy Eric Schalnat, Group 42, Inc.
   May 25, 1996
   */
#define PNG_INTERNAL
#include "png.h"

/* place a 32 bit number into a buffer in png byte order.  We work
   with unsigned numbers for convenience, you may have to cast
   signed numbers (if you use any, most png data is unsigned). */
void
png_save_uint_32(png_bytep buf, png_uint_32 i)
{
   buf[0] = (png_byte)((i >> 24) & 0xff);
   buf[1] = (png_byte)((i >> 16) & 0xff);
   buf[2] = (png_byte)((i >> 8) & 0xff);
   buf[3] = (png_byte)(i & 0xff);
}

/* place a 16 bit number into a buffer in png byte order */
void
png_save_uint_16(png_bytep buf, png_uint_16 i)
{
   buf[0] = (png_byte)((i >> 8) & 0xff);
   buf[1] = (png_byte)(i & 0xff);
}

/* write a 32 bit number */
void
png_write_uint_32(png_structp png_ptr, png_uint_32 i)
{
   png_byte buf[4];

   buf[0] = (png_byte)((i >> 24) & 0xff);
   buf[1] = (png_byte)((i >> 16) & 0xff);
   buf[2] = (png_byte)((i >> 8) & 0xff);
   buf[3] = (png_byte)(i & 0xff);
   png_write_data(png_ptr, buf, 4);
}

/* write a 16 bit number */
void
png_write_uint_16(png_structp png_ptr, png_uint_16 i)
{
   png_byte buf[2];

   buf[0] = (png_byte)((i >> 8) & 0xff);
   buf[1] = (png_byte)(i & 0xff);
   png_write_data(png_ptr, buf, 2);
}

/* Write a png chunk all at once.  The type is an array of ASCII characters
   representing the chunk name.  The array must be at least 4 bytes in
   length, and does not need to be null terminated.  To be safe, pass the
   pre-defined chunk names here, and if you need a new one, define it
   where the others are defined.  The length is the length of the data.
   All the data must be present.  If that is not possible, use the
   png_write_chunk_start(), png_write_chunk_data(), and png_write_chunk_end()
   functions instead.  */
void
png_write_chunk(png_structp png_ptr, png_bytep type,
   png_bytep data, png_uint_32 length)
{
   /* write length */
   png_write_uint_32(png_ptr, length);
   /* write chunk name */
   png_write_data(png_ptr, type, (png_uint_32)4);
   /* reset the crc and run the chunk name over it */
   png_reset_crc(png_ptr);
   png_calculate_crc(png_ptr, type, (png_uint_32)4);
   /* write the data and update the crc */
   if (length)
   {
      png_calculate_crc(png_ptr, data, length);
      png_write_data(png_ptr, data, length);
   }
   /* write the crc */
   png_write_uint_32(png_ptr, ~png_ptr->crc);
}

/* Write the start of a png chunk.  The type is the chunk type.
   The total_length is the sum of the lengths of all the data you will be
   passing in png_write_chunk_data() */
void
png_write_chunk_start(png_structp png_ptr, png_bytep type,
   png_uint_32 total_length)
{
   /* write the length */
   png_write_uint_32(png_ptr, total_length);
   /* write the chunk name */
   png_write_data(png_ptr, type, (png_uint_32)4);
   /* reset the crc and run it over the chunk name */
   png_reset_crc(png_ptr);
   png_calculate_crc(png_ptr, type, (png_uint_32)4);
}

/* write the data of a png chunk started with png_write_chunk_start().
   Note that multiple calls to this function are allowed, and that the
   sum of the lengths from these calls *must* add up to the total_length
   given to png_write_chunk_start() */
void
png_write_chunk_data(png_structp png_ptr, png_bytep data, png_uint_32 length)
{
   /* write the data, and run the crc over it */
   if (length)
   {
      png_calculate_crc(png_ptr, data, length);
      png_write_data(png_ptr, data, length);
   }
}

/* finish a chunk started with png_write_chunk_start() */
void
png_write_chunk_end(png_structp png_ptr)
{
   /* write the crc */
   png_write_uint_32(png_ptr, ~png_ptr->crc);
}

/* simple function to write the signature */
void
png_write_sig(png_structp png_ptr)
{
   /* write the 8 byte signature */
   png_write_data(png_ptr, png_sig, (png_uint_32)8);
}

/* Write the IHDR chunk, and update the png_struct with the necessary
   information.  Note that the rest of this code depends upon this
   information being correct.  */
void
png_write_IHDR(png_structp png_ptr, png_uint_32 width, png_uint_32 height,
   int bit_depth, int color_type, int compression_type, int filter_type,
   int interlace_type)
{
   png_byte buf[13]; /* buffer to store the IHDR info */

   /* Check that we have valid input data from the application info */
   switch (color_type)
   {
      case 0:
         switch (bit_depth)
         {
            case 1:
            case 2:
            case 4:
            case 8:
            case 16: png_ptr->channels = 1; break;
            default: png_error(png_ptr, "Invalid bit depth for grayscale image");
         }
         break;
      case 2:
         if (bit_depth != 8 && bit_depth != 16)
            png_error(png_ptr, "Invalid bit depth for RGB image");
         png_ptr->channels = 3;
         break;
      case 3:
         switch (bit_depth)
         {
            case 1:
            case 2:
            case 4:
            case 8: png_ptr->channels = 1; break;
            default: png_error(png_ptr, "Invalid bit depth for paletted image");
         }
         break;
      case 4:
         if (bit_depth != 8 && bit_depth != 16)
            png_error(png_ptr, "Invalid bit depth for grayscale+alpha image");
         png_ptr->channels = 2;
         break;
      case 6:
         if (bit_depth != 8 && bit_depth != 16)
            png_error(png_ptr, "Invalid bit depth for RGBA image");
         png_ptr->channels = 4;
         break;
      default:
         png_error(png_ptr, "Invalid image color type specified");
   }

   if (compression_type != 0)
   {
      png_warning(png_ptr, "Invalid compression type specified");
      compression_type = 0;
   }

   if (filter_type != 0)
   {
      png_warning(png_ptr, "Invalid filter type specified");
      filter_type = 0;
   }

   if (interlace_type != 0 && interlace_type != 1)
   {
      png_warning(png_ptr, "Invalid interlace type specified");
      interlace_type = 1;
   }

   /* save off the relevent information */
   png_ptr->bit_depth = (png_byte)bit_depth;
   png_ptr->color_type = (png_byte)color_type;
   png_ptr->interlaced = (png_byte)interlace_type;
   png_ptr->width = width;
   png_ptr->height = height;

   png_ptr->pixel_depth = (png_byte)(bit_depth * png_ptr->channels);
   png_ptr->rowbytes = ((width * (png_uint_32)png_ptr->pixel_depth + 7) >> 3);
   /* set the usr info, so any transformations can modify it */
   png_ptr->usr_width = png_ptr->width;
   png_ptr->usr_bit_depth = png_ptr->bit_depth;
   png_ptr->usr_channels = png_ptr->channels;

   /* pack the header information into the buffer */
   png_save_uint_32(buf, width);
   png_save_uint_32(buf + 4, height);
   buf[8] = (png_byte)bit_depth;
   buf[9] = (png_byte)color_type;
   buf[10] = (png_byte)compression_type;
   buf[11] = (png_byte)filter_type;
   buf[12] = (png_byte)interlace_type;

   /* write the chunk */
   png_write_chunk(png_ptr, png_IHDR, buf, (png_uint_32)13);

   /* initialize zlib with png info */
   png_ptr->zstream = (z_stream *)png_malloc(png_ptr, sizeof (z_stream));
   png_ptr->zstream->zalloc = png_zalloc;
   png_ptr->zstream->zfree = png_zfree;
   png_ptr->zstream->opaque = (voidpf)png_ptr;
   if (!(png_ptr->do_filter))
   {
      if (png_ptr->color_type == 3 || png_ptr->bit_depth < 8)
         png_ptr->do_filter = PNG_FILTER_NONE;
      else
         png_ptr->do_filter = PNG_ALL_FILTERS;
   }
   if (!(png_ptr->flags & PNG_FLAG_ZLIB_CUSTOM_STRATEGY))
   {
      if (png_ptr->do_filter != PNG_FILTER_NONE)
         png_ptr->zlib_strategy = Z_FILTERED;
      else
         png_ptr->zlib_strategy = Z_DEFAULT_STRATEGY;
   }
   if (!(png_ptr->flags & PNG_FLAG_ZLIB_CUSTOM_LEVEL))
      png_ptr->zlib_level = Z_DEFAULT_COMPRESSION;
   if (!(png_ptr->flags & PNG_FLAG_ZLIB_CUSTOM_MEM_LEVEL))
      png_ptr->zlib_mem_level = 8;
   if (!(png_ptr->flags & PNG_FLAG_ZLIB_CUSTOM_WINDOW_BITS))
      png_ptr->zlib_window_bits = 15;
   if (!(png_ptr->flags & PNG_FLAG_ZLIB_CUSTOM_METHOD))
      png_ptr->zlib_method = 8;
   deflateInit2(png_ptr->zstream, png_ptr->zlib_level,
      png_ptr->zlib_method,
      png_ptr->zlib_window_bits,
      png_ptr->zlib_mem_level,
      png_ptr->zlib_strategy);
   png_ptr->zstream->next_out = png_ptr->zbuf;
   png_ptr->zstream->avail_out = (uInt)png_ptr->zbuf_size;

   png_ptr->mode = PNG_HAVE_IHDR;
}

/* write the palette.  We are careful not to trust png_color to be in the
   correct order for PNG, so people can redefine it to any convient
   structure. */
void
png_write_PLTE(png_structp png_ptr, png_colorp palette, int number)
{
   int i;
   png_colorp pal_ptr;
   png_byte buf[3];

   if (number == 0 || number > 256)
   {
      if (png_ptr->color_type == PNG_COLOR_TYPE_PALETTE)
      {
         png_error(png_ptr, "Invalid number of colors in palette");
      }
      else
      {
         png_warning(png_ptr, "Invalid number of colors in palette");
         return;
      }
   }

   png_ptr->num_palette = number;

   png_write_chunk_start(png_ptr, png_PLTE, number * 3);
   for (i = 0, pal_ptr = palette;
      i < number;
      i++, pal_ptr++)
   {
      buf[0] = pal_ptr->red;
      buf[1] = pal_ptr->green;
      buf[2] = pal_ptr->blue;
      png_write_chunk_data(png_ptr, buf, (png_uint_32)3);
   }
   png_write_chunk_end(png_ptr);
   png_ptr->mode |= PNG_HAVE_PLTE;
}

/* write an IDAT chunk */
void
png_write_IDAT(png_structp png_ptr, png_bytep data, png_uint_32 length)
{
   png_write_chunk(png_ptr, png_IDAT, data, length);
   png_ptr->mode |= PNG_HAVE_IDAT;
}

/* write an IEND chunk */
void
png_write_IEND(png_structp png_ptr)
{
   png_write_chunk(png_ptr, png_IEND, NULL, (png_uint_32)0);
   png_ptr->mode |= PNG_AFTER_IEND;
}

#if defined(PNG_WRITE_gAMA_SUPPORTED)
/* write a gAMA chunk */
void
png_write_gAMA(png_structp png_ptr, double gamma)
{
   png_uint_32 igamma;
   png_byte buf[4];

   /* gamma is saved in 1/100,000ths */
   igamma = (png_uint_32)(gamma * 100000.0 + 0.5);
   png_save_uint_32(buf, igamma);
   png_write_chunk(png_ptr, png_gAMA, buf, (png_uint_32)4);
}
#endif

#if defined(PNG_WRITE_sBIT_SUPPORTED)
/* write the sBIT chunk */
void
png_write_sBIT(png_structp png_ptr, png_color_8p sbit, int color_type)
{
   png_byte buf[4];
   int size;

   /* make sure we don't depend upon the order of PNG_COLOR_8 */
   if (color_type & PNG_COLOR_MASK_COLOR)
   {
      int maxbits;

      maxbits = color_type==PNG_COLOR_TYPE_PALETTE ? 8:png_ptr->usr_bit_depth;
      if (sbit->red == 0 || sbit->red > maxbits || 
          sbit->green == 0 || sbit->green > maxbits || 
          sbit->blue == 0 || sbit->blue > maxbits)
      {
         png_warning(png_ptr, "Invalid sBIT depth specified");
         return;
      }
      buf[0] = sbit->red;
      buf[1] = sbit->green;
      buf[2] = sbit->blue;
      size = 3;
   }
   else
   {
      if (sbit->gray == 0 || sbit->gray > png_ptr->usr_bit_depth)
      {
         png_warning(png_ptr, "Invalid sBIT depth specified");
         return;
      }
      buf[0] = sbit->gray;
      size = 1;
   }

   if (color_type & PNG_COLOR_MASK_ALPHA)
   {
      if (sbit->alpha == 0 || sbit->alpha > png_ptr->usr_bit_depth)
      {
         png_warning(png_ptr, "Invalid sBIT depth specified");
         return;
      }
      buf[size++] = sbit->alpha;
   }

   png_write_chunk(png_ptr, png_sBIT, buf, (png_uint_32)size);
}
#endif

#if defined(PNG_WRITE_cHRM_SUPPORTED)
/* write the cHRM chunk */
void
png_write_cHRM ( png_structp png_ptr, double white_x, double white_y,
   double red_x, double red_y, double green_x, double green_y,
   double blue_x, double blue_y)
{
   png_uint_32 itemp;
   png_byte buf[32];

   /* each value is saved int 1/100,000ths */
   if (white_x < 0 || white_x > 0.8 || white_y < 0 || white_y > 0.8 ||
       white_x + white_y > 1.0)
   {
      png_warning(png_ptr, "Invalid cHRM white point specified");
      return;
   }
   itemp = (png_uint_32)(white_x * 100000.0 + 0.5);
   png_save_uint_32(buf, itemp);
   itemp = (png_uint_32)(white_y * 100000.0 + 0.5);
   png_save_uint_32(buf + 4, itemp);

   if (red_x < 0 || red_x > 0.8 || red_y < 0 || red_y > 0.8 ||
       red_x + red_y > 1.0)
   {
      png_warning(png_ptr, "Invalid cHRM red point specified");
      return;
   }
   itemp = (png_uint_32)(red_x * 100000.0 + 0.5);
   png_save_uint_32(buf + 8, itemp);
   itemp = (png_uint_32)(red_y * 100000.0 + 0.5);
   png_save_uint_32(buf + 12, itemp);

   if (green_x < 0 || green_x > 0.8 || green_y < 0 || green_y > 0.8 ||
       green_x + green_y > 1.0)
   {
      png_warning(png_ptr, "Invalid cHRM green point specified");
      return;
   }
   itemp = (png_uint_32)(green_x * 100000.0 + 0.5);
   png_save_uint_32(buf + 16, itemp);
   itemp = (png_uint_32)(green_y * 100000.0 + 0.5);
   png_save_uint_32(buf + 20, itemp);

   if (blue_x < 0 || blue_x > 0.8 || blue_y < 0 || blue_y > 0.8 ||
       blue_x + blue_y > 1.0)
   {
      png_warning(png_ptr, "Invalid cHRM blue point specified");
      return;
   }
   itemp = (png_uint_32)(blue_x * 100000.0 + 0.5);
   png_save_uint_32(buf + 24, itemp);
   itemp = (png_uint_32)(blue_y * 100000.0 + 0.5);
   png_save_uint_32(buf + 28, itemp);

   png_write_chunk(png_ptr, png_cHRM, buf, (png_uint_32)32);
}
#endif

#if defined(PNG_WRITE_tRNS_SUPPORTED)
/* write the tRNS chunk */
void
png_write_tRNS(png_structp png_ptr, png_bytep trans, png_color_16p tran,
   int num_trans, int color_type)
{
   png_byte buf[6];

   if (color_type == PNG_COLOR_TYPE_PALETTE)
   {
      if (num_trans <= 0 || num_trans > png_ptr->num_palette)
      {
         png_warning(png_ptr,"Invalid number of transparent colors specified");
         return;
      }
      /* write the chunk out as it is */
      png_write_chunk(png_ptr, png_tRNS, trans, (png_uint_32)num_trans);
   }
   else if (color_type == PNG_COLOR_TYPE_GRAY)
   {
      /* one 16 bit value */
      png_save_uint_16(buf, tran->gray);
      png_write_chunk(png_ptr, png_tRNS, buf, (png_uint_32)2);
   }
   else if (color_type == PNG_COLOR_TYPE_RGB)
   {
      /* three 16 bit values */
      png_save_uint_16(buf, tran->red);
      png_save_uint_16(buf + 2, tran->green);
      png_save_uint_16(buf + 4, tran->blue);
      png_write_chunk(png_ptr, png_tRNS, buf, (png_uint_32)6);
   }
   else
   {
      png_warning(png_ptr, "Can't write tRNS with and alpha channel");
   }
}
#endif

#if defined(PNG_WRITE_bKGD_SUPPORTED)
/* write the background chunk */
void
png_write_bKGD(png_structp png_ptr, png_color_16p back, int color_type)
{
   png_byte buf[6];

   if (color_type == PNG_COLOR_TYPE_PALETTE)
   {
      if (back->index > png_ptr->num_palette)
      {
         png_warning(png_ptr, "Invalid background palette index");
         return;
      }
      buf[0] = back->index;
      png_write_chunk(png_ptr, png_bKGD, buf, (png_uint_32)1);
   }
   else if (color_type & PNG_COLOR_MASK_COLOR)
   {
      png_save_uint_16(buf, back->red);
      png_save_uint_16(buf + 2, back->green);
      png_save_uint_16(buf + 4, back->blue);
      png_write_chunk(png_ptr, png_bKGD, buf, (png_uint_32)6);
   }
   else
   {
      png_save_uint_16(buf, back->gray);
      png_write_chunk(png_ptr, png_bKGD, buf, (png_uint_32)2);
   }
}
#endif

#if defined(PNG_WRITE_hIST_SUPPORTED)
/* write the histogram */
void
png_write_hIST(png_structp png_ptr, png_uint_16p hist, int number)
{
   int i;
   png_byte buf[3];

   if (number <= 0 || number > png_ptr->num_palette)
   {
      png_warning(png_ptr, "Invalid number of histogram entries specified");
      return;
   }

   png_write_chunk_start(png_ptr, png_hIST, (png_uint_32)(number * 2));
   for (i = 0; i < number; i++)
   {
      png_save_uint_16(buf, hist[i]);
      png_write_chunk_data(png_ptr, buf, (png_uint_32)2);
   }
   png_write_chunk_end(png_ptr);
}
#endif

#if defined(PNG_WRITE_tEXt_SUPPORTED)
/* write a tEXt chunk */
void
png_write_tEXt(png_structp png_ptr, png_charp key, png_charp text,
   png_uint_32 text_len)
{
   int key_len;

   key_len = png_strlen(key);

   if (key_len == 0)
   {
      png_warning(png_ptr, "Invalid text keyword length");
      return;
   }
   else if (key_len > 80)
   {
      png_warning(png_ptr, "Text keyword length restricted to 80 characters\n");
      key[80] = '\0';
      key_len = 80;
   }

   /* make sure we count the 0 after the key */
   png_write_chunk_start(png_ptr, png_tEXt,
      (png_uint_32)(key_len + text_len + 1));
   /* key has an 0 at the end.  How nice */
   png_write_chunk_data(png_ptr, (png_bytep )key, (png_uint_32)(key_len + 1));
   if (text && text_len)
      png_write_chunk_data(png_ptr, (png_bytep )text, (png_uint_32)text_len);
   png_write_chunk_end(png_ptr);
}
#endif

#if defined(PNG_WRITE_zTXt_SUPPORTED)
/* write a compressed chunk */
void
png_write_zTXt(png_structp png_ptr, png_charp key, png_charp text,
   png_uint_32 text_len, int compression)
{
   int key_len;
   char buf[1];
   int i, ret;
   png_charpp output_ptr = NULL; /* array of pointers to output */
   int num_output_ptr = 0; /* number of output pointers used */
   int max_output_ptr = 0; /* size of output_ptr */

   key_len = png_strlen(key);

   if (key_len == 0)
   {
      png_warning(png_ptr, "Invalid text keyword length");
      return;
   }
   else if (key_len > 80)
   {
      png_warning(png_ptr, "Text keyword length restricted to 80 characters\n");
      key[80] = '\0';
      key_len = 80;
   }

   if (compression != 0)
   {
      png_warning(png_ptr, "Only type 0 compression allowed for text\n");
      compression = 0;
   }

   /* we can't write the chunk until we find out how much data we have,
      which means we need to run the compresser first, and save the
      output.  This shouldn't be a problem, as the vast majority of
      comments should be reasonable, but we will set up an array of
      malloced pointers to be sure. */

   /* set up the compression buffers */
   png_ptr->zstream->avail_in = (uInt)text_len;
   png_ptr->zstream->next_in = (Bytef *)text;
   png_ptr->zstream->avail_out = (uInt)png_ptr->zbuf_size;
   png_ptr->zstream->next_out = (Bytef *)png_ptr->zbuf;

   /* this is the same compression loop as in png_write_row() */
   do
   {
      /* compress the data */
      ret = deflate(png_ptr->zstream, Z_NO_FLUSH);
      if (ret != Z_OK)
      {
         /* error */
         if (png_ptr->zstream->msg)
            png_error(png_ptr, png_ptr->zstream->msg);
         else
            png_error(png_ptr, "zlib error");
      }
      /* check to see if we need more room */
      if (!png_ptr->zstream->avail_out && png_ptr->zstream->avail_in)
      {
         /* make sure the output array has room */
         if (num_output_ptr >= max_output_ptr)
         {
            png_uint_32 old_max;

            old_max = max_output_ptr;
            max_output_ptr = num_output_ptr + 4;
            if (output_ptr)
            {
               png_charpp old_ptr;

               old_ptr = output_ptr;
               output_ptr = (png_charpp)png_large_malloc(png_ptr,
                  max_output_ptr * sizeof (png_charpp));
               png_memcpy(output_ptr, old_ptr,
                  (png_size_t)(old_max * sizeof (png_charp)));
               png_large_free(png_ptr, old_ptr);
            }
            else
               output_ptr = (png_charpp)png_large_malloc(png_ptr,
                  max_output_ptr * sizeof (png_charp));
         }

         /* save the data */
         output_ptr[num_output_ptr] = png_large_malloc(png_ptr,
            png_ptr->zbuf_size);
         png_memcpy(output_ptr[num_output_ptr], png_ptr->zbuf,
            (png_size_t)png_ptr->zbuf_size);
         num_output_ptr++;

         /* and reset the buffer */
         png_ptr->zstream->avail_out = (uInt)png_ptr->zbuf_size;
         png_ptr->zstream->next_out = png_ptr->zbuf;
      }
   /* continue until we don't have anymore to compress */
   } while (png_ptr->zstream->avail_in);

   /* finish the compression */
   do
   {
      /* tell zlib we are finished */
      ret = deflate(png_ptr->zstream, Z_FINISH);
      if (ret != Z_OK && ret != Z_STREAM_END)
      {
         /* we got an error */
         if (png_ptr->zstream->msg)
            png_error(png_ptr, png_ptr->zstream->msg);
         else
            png_error(png_ptr, "zlib error");
      }

      /* check to see if we need more room */
      if (!png_ptr->zstream->avail_out && ret == Z_OK)
      {
         /* check to make sure our output array has room */
         if (num_output_ptr >= max_output_ptr)
         {
            png_uint_32 old_max;

            old_max = max_output_ptr;
            max_output_ptr = num_output_ptr + 4;
            if (output_ptr)
            {
               png_charpp old_ptr;

               old_ptr = output_ptr;
               output_ptr = (png_charpp)png_large_malloc(png_ptr,
                  max_output_ptr * sizeof (png_charpp));
               png_memcpy(output_ptr, old_ptr,
                  (png_size_t)(old_max * sizeof (png_charp)));
               png_large_free(png_ptr, old_ptr);
            }
            else
               output_ptr = (png_charpp)png_large_malloc(png_ptr,
                  max_output_ptr * sizeof (png_charp));
         }

         /* save off the data */
         output_ptr[num_output_ptr] = png_large_malloc(png_ptr,
            png_ptr->zbuf_size);
         png_memcpy(output_ptr[num_output_ptr], png_ptr->zbuf,
            (png_size_t)png_ptr->zbuf_size);
         num_output_ptr++;

         /* and reset the buffer pointers */
         png_ptr->zstream->avail_out = (uInt)png_ptr->zbuf_size;
         png_ptr->zstream->next_out = png_ptr->zbuf;
      }
   } while (ret != Z_STREAM_END);

   /* text length is number of buffers plus last buffer */
   text_len = png_ptr->zbuf_size * num_output_ptr;
   if (png_ptr->zstream->avail_out < png_ptr->zbuf_size)
      text_len += (png_uint_32)(png_ptr->zbuf_size -
         png_ptr->zstream->avail_out);

   /* write start of chunk */
   png_write_chunk_start(png_ptr, png_zTXt,
      (png_uint_32)(key_len + text_len + 2));
   /* write key */
   png_write_chunk_data(png_ptr, (png_bytep )key, (png_uint_32)(key_len + 1));
   buf[0] = (png_byte)compression;
   /* write compression */
   png_write_chunk_data(png_ptr, (png_bytep )buf, (png_uint_32)1);

   /* write saved output buffers, if any */
   for (i = 0; i < num_output_ptr; i++)
   {
      png_write_chunk_data(png_ptr, (png_bytep )output_ptr[i], png_ptr->zbuf_size);
      png_large_free(png_ptr, output_ptr[i]);
   }
   if (max_output_ptr)
      png_large_free(png_ptr, output_ptr);
   /* write anything left in zbuf */
   if (png_ptr->zstream->avail_out < png_ptr->zbuf_size)
      png_write_chunk_data(png_ptr, png_ptr->zbuf,
         png_ptr->zbuf_size - png_ptr->zstream->avail_out);
   /* close the chunk */
   png_write_chunk_end(png_ptr);

   /* reset zlib for another zTXt or the image data */
   deflateReset(png_ptr->zstream);
}
#endif

#if defined(PNG_WRITE_pHYs_SUPPORTED)
/* write the pHYs chunk */
void
png_write_pHYs(png_structp png_ptr, png_uint_32 x_pixels_per_unit,
   png_uint_32 y_pixels_per_unit,
   int unit_type)
{
   png_byte buf[9];

   if (unit_type >= PNG_RESOLUTION_LAST)
      png_warning(png_ptr, "Unrecognized unit type for pHYs chunk");

   png_save_uint_32(buf, x_pixels_per_unit);
   png_save_uint_32(buf + 4, y_pixels_per_unit);
   buf[8] = (png_byte)unit_type;

   png_write_chunk(png_ptr, png_pHYs, buf, (png_uint_32)9);
}
#endif

#if defined(PNG_WRITE_oFFs_SUPPORTED)
/* write the oFFs chunk */
void
png_write_oFFs(png_structp png_ptr, png_uint_32 x_offset,
   png_uint_32 y_offset,
   int unit_type)
{
   png_byte buf[9];

   if (unit_type >= PNG_OFFSET_LAST)
      png_warning(png_ptr, "Unrecognized unit type for oFFs chunk");

   png_save_uint_32(buf, x_offset);
   png_save_uint_32(buf + 4, y_offset);
   buf[8] = (png_byte)unit_type;

   png_write_chunk(png_ptr, png_oFFs, buf, (png_uint_32)9);
}
#endif

#if defined(PNG_WRITE_tIME_SUPPORTED)
/* write the tIME chunk.  Use either png_convert_from_struct_tm()
   or png_convert_from_time_t(), or fill in the structure yourself */
void
png_write_tIME(png_structp png_ptr, png_timep mod_time)
{
   png_byte buf[7];

   if (mod_time->month  > 12 || mod_time->month  < 1 ||
       mod_time->day    > 31 || mod_time->day    < 1 ||
       mod_time->hour   > 23 || mod_time->second > 60)
   {
      png_warning(png_ptr, "Invalid time specified for tIME chunk");
      return;
   }

   png_save_uint_16(buf, mod_time->year);
   buf[2] = mod_time->month;
   buf[3] = mod_time->day;
   buf[4] = mod_time->hour;
   buf[5] = mod_time->minute;
   buf[6] = mod_time->second;

   png_write_chunk(png_ptr, png_tIME, buf, (png_uint_32)7);
}
#endif

/* initializes the row writing capability of libpng */
void
png_write_start_row(png_structp png_ptr)
{
   /* set up row buffer */
   png_ptr->row_buf = (png_bytep )png_large_malloc(png_ptr,
      (((png_uint_32)png_ptr->usr_channels *
      (png_uint_32)png_ptr->usr_bit_depth *
      png_ptr->width + 7) >> 3) + 1);
   png_ptr->row_buf[0] = 0;

   /* set up filtering buffer, if using this filter */
   if (png_ptr->do_filter & PNG_FILTER_SUB)
   {
      png_ptr->sub_row = (png_bytep )png_large_malloc(png_ptr,
         png_ptr->rowbytes + 1);
      png_ptr->sub_row[0] = 1;  /* Set the row filter type */
   }

   /* We only need to keep the previous row if we are using one of these */
   if (png_ptr->do_filter & (PNG_FILTER_AVG | PNG_FILTER_UP | PNG_FILTER_PAETH))
   {
     /* set up previous row buffer */
      png_ptr->prev_row = (png_bytep )png_large_malloc(png_ptr,
         (((png_uint_32)png_ptr->usr_channels *
         (png_uint_32)png_ptr->usr_bit_depth *
         png_ptr->width + 7) >> 3) + 1);
      png_memset(png_ptr->prev_row, 0, (((png_uint_32)png_ptr->usr_channels *
         (png_uint_32)png_ptr->usr_bit_depth * png_ptr->width + 7) >> 3) + 1);

      if (png_ptr->do_filter & PNG_FILTER_UP)
      {
         png_ptr->up_row = (png_bytep )png_large_malloc(png_ptr,
            png_ptr->rowbytes + 1);
         png_ptr->up_row[0] = 2;  /* Set the row filter type */
      }

      if (png_ptr->do_filter & PNG_FILTER_AVG)
      {
         png_ptr->avg_row = (png_bytep )png_large_malloc(png_ptr,
            png_ptr->rowbytes + 1);
         png_ptr->avg_row[0] = 3;  /* Set the row filter type */
      }

      if (png_ptr->do_filter & PNG_FILTER_PAETH)
      {
         png_ptr->paeth_row = (png_bytep )png_large_malloc(png_ptr,
            png_ptr->rowbytes + 1);
         png_ptr->paeth_row[0] = 4;  /* Set the row filter type */
      }
   }

   /* if interlaced, we need to set up width and height of pass */
   if (png_ptr->interlaced)
   {
      if (!(png_ptr->transformations & PNG_INTERLACE))
      {
         png_ptr->num_rows = (png_ptr->height + png_pass_yinc[0] - 1 -
            png_pass_ystart[0]) / png_pass_yinc[0];
         png_ptr->usr_width = (png_ptr->width +
            png_pass_inc[0] - 1 -
            png_pass_start[0]) /
            png_pass_inc[0];
      }
      else
      {
         png_ptr->num_rows = png_ptr->height;
         png_ptr->usr_width = png_ptr->width;
      }
   }
   else
   {
      png_ptr->num_rows = png_ptr->height;
      png_ptr->usr_width = png_ptr->width;
   }
   png_ptr->zstream->avail_out = (uInt)png_ptr->zbuf_size;
   png_ptr->zstream->next_out = png_ptr->zbuf;
}

/* Internal use only.   Called when finished processing a row of data */
void
png_write_finish_row(png_structp png_ptr)
{
   int ret;

   /* next row */
   png_ptr->row_number++;

   /* see if we are done */
   if (png_ptr->row_number < png_ptr->num_rows)
      return;

   /* if interlaced, go to next pass */
   if (png_ptr->interlaced)
   {
      png_ptr->row_number = 0;
      if (png_ptr->transformations & PNG_INTERLACE)
      {
         png_ptr->pass++;
      }
      else
      {
         /* loop until we find a non-zero width or height pass */
         do
         {
            png_ptr->pass++;
            if (png_ptr->pass >= 7)
               break;
            png_ptr->usr_width = (png_ptr->width +
               png_pass_inc[png_ptr->pass] - 1 -
               png_pass_start[png_ptr->pass]) /
               png_pass_inc[png_ptr->pass];
            png_ptr->num_rows = (png_ptr->height +
               png_pass_yinc[png_ptr->pass] - 1 -
               png_pass_ystart[png_ptr->pass]) /
               png_pass_yinc[png_ptr->pass];
            if (png_ptr->transformations & PNG_INTERLACE)
               break;
         } while (png_ptr->usr_width == 0 || png_ptr->num_rows == 0);

      }

      /* reset the row above the image for the next pass */
      if (png_ptr->pass < 7)
      {
         if (png_ptr->prev_row)
            png_memset(png_ptr->prev_row, 0,
               (((png_uint_32)png_ptr->usr_channels *
               (png_uint_32)png_ptr->usr_bit_depth *
               png_ptr->width + 7) >> 3) + 1);
         return;
      }
   }

   /* if we get here, we've just written the last row, so we need
      to flush the compressor */
   do
   {
      /* tell the compressor we are done */
      ret = deflate(png_ptr->zstream, Z_FINISH);
      /* check for an error */
      if (ret != Z_OK && ret != Z_STREAM_END)
      {
         if (png_ptr->zstream->msg)
            png_error(png_ptr, png_ptr->zstream->msg);
         else
            png_error(png_ptr, "zlib error");
      }
      /* check to see if we need more room */
      if (!png_ptr->zstream->avail_out && ret == Z_OK)
      {
         png_write_IDAT(png_ptr, png_ptr->zbuf, png_ptr->zbuf_size);
         png_ptr->zstream->next_out = png_ptr->zbuf;
         png_ptr->zstream->avail_out = (uInt)png_ptr->zbuf_size;
      }
   } while (ret != Z_STREAM_END);

   /* write any extra space */
   if (png_ptr->zstream->avail_out < png_ptr->zbuf_size)
   {
      png_write_IDAT(png_ptr, png_ptr->zbuf, png_ptr->zbuf_size -
         png_ptr->zstream->avail_out);
   }

   deflateReset(png_ptr->zstream);
}

#if defined(PNG_WRITE_INTERLACING_SUPPORTED)
/* pick out the correct pixels for the interlace pass.

   The basic idea here is to go through the row with a source
   pointer and a destination pointer (sp and dp), and copy the
   correct pixels for the pass.  As the row gets compacted,
   sp will always be >= dp, so we should never overwrite anything.
   See the default: case for the easiest code to understand.
   */
void
png_do_write_interlace(png_row_infop row_info, png_bytep row, int pass)
{
   /* we don't have to do anything on the last pass (6) */
   if (row && row_info && pass < 6)
   {
      /* each pixel depth is handled seperately */
      switch (row_info->pixel_depth)
      {
         case 1:
         {
            png_bytep sp;
            png_bytep dp;
            int shift;
            int d;
            int value;
            png_uint_32 i;

            dp = row;
            d = 0;
            shift = 7;
            for (i = png_pass_start[pass];
               i < row_info->width;
               i += png_pass_inc[pass])
            {
               sp = row + (png_size_t)(i >> 3);
               value = (int)(*sp >> (7 - (int)(i & 7))) & 0x1;
               d |= (value << shift);

               if (shift == 0)
               {
                  shift = 7;
                  *dp++ = (png_byte)d;
                  d = 0;
               }
               else
                  shift--;

            }
            if (shift != 7)
               *dp = (png_byte)d;
            break;
         }
         case 2:
         {
            png_bytep sp;
            png_bytep dp;
            int shift;
            int d;
            int value;
            png_uint_32 i;

            dp = row;
            shift = 6;
            d = 0;
            for (i = png_pass_start[pass];
               i < row_info->width;
               i += png_pass_inc[pass])
            {
               sp = row + (png_size_t)(i >> 2);
               value = (*sp >> ((3 - (int)(i & 3)) << 1)) & 0x3;
               d |= (value << shift);

               if (shift == 0)
               {
                  shift = 6;
                  *dp++ = (png_byte)d;
                  d = 0;
               }
               else
                  shift -= 2;
            }
            if (shift != 6)
                   *dp = (png_byte)d;
            break;
         }
         case 4:
         {
            png_bytep sp;
            png_bytep dp;
            int shift;
            int d;
            int value;
            png_uint_32 i;

            dp = row;
            shift = 4;
            d = 0;
            for (i = png_pass_start[pass];
               i < row_info->width;
               i += png_pass_inc[pass])
            {
               sp = row + (png_size_t)(i >> 1);
               value = (*sp >> ((1 - (int)(i & 1)) << 2)) & 0xf;
               d |= (value << shift);

               if (shift == 0)
               {
                  shift = 4;
                  *dp++ = (png_byte)d;
                  d = 0;
               }
               else
                  shift -= 4;
            }
            if (shift != 4)
               *dp = (png_byte)d;
            break;
         }
         default:
         {
            png_bytep sp;
            png_bytep dp;
            png_uint_32 i;
            int pixel_bytes;

            /* start at the beginning */
            dp = row;
            /* find out how many bytes each pixel takes up */
            pixel_bytes = (row_info->pixel_depth >> 3);
            /* loop through the row, only looking at the pixels that
               matter */
            for (i = png_pass_start[pass];
               i < row_info->width;
               i += png_pass_inc[pass])
            {
               /* find out where the original pixel is */
               sp = row + (png_size_t)(i * pixel_bytes);
               /* move the pixel */
               if (dp != sp)
                  png_memcpy(dp, sp, pixel_bytes);
               /* next pixel */
               dp += pixel_bytes;
            }
            break;
         }
      }
      /* set new row width */
      row_info->width = (row_info->width +
         png_pass_inc[pass] - 1 -
         png_pass_start[pass]) /
         png_pass_inc[pass];
      row_info->rowbytes = ((row_info->width *
         row_info->pixel_depth + 7) >> 3);

   }
}
#endif

/* this filters the row, chooses which filter to use, if it has not already
 * been given by the application, and then writes the row out with the
 * chosen filter */
void
png_write_find_filter(png_structp png_ptr, png_row_infop row_info)
{
   png_bytep prev_row, best_row, row_buf;
   png_uint_32 mins;
   int bpp;

   /* find out how many bytes offset each pixel is */
   bpp = (row_info->pixel_depth + 7) / 8;

   prev_row = png_ptr->prev_row;
   best_row = row_buf = png_ptr->row_buf;
   mins = 0xffffffff;

   /* the prediction method we use is to find which method provides
      the smallest value when summing the abs of the distances from
      zero using anything >= 128 as negitive numbers. */

   /* We don't need to test the 'no filter' case if this is the only filter
    * that has been chosen, as it doesn't actually do anything to the data. */
   if (png_ptr->do_filter & PNG_FILTER_NONE &&
       png_ptr->do_filter != PNG_FILTER_NONE)
   {
      png_bytep rp;
      png_uint_32 sum = 0;
      int i, v;

      for (i = 0, rp = row_buf + 1; i < row_info->rowbytes; i++, rp++)
      {
         v = *rp;
         sum += (v < 128) ? v : 256 - v;
      }
      mins = sum;
   }

   /* sub filter */
   if (png_ptr->do_filter & PNG_FILTER_SUB)
   {
      png_bytep rp, dp, lp;
      png_uint_32 sum = 0;
      int i, v;

      for (i = 0, rp = row_buf + 1, dp = png_ptr->sub_row + 1; i < bpp;
           i++, rp++, dp++)
      {
         v = *dp = *rp;

         sum += (v < 128) ? v : 256 - v;
      }
      for (lp = row_buf + 1; i < row_info->rowbytes; i++, rp++, lp++, dp++)
      {
         v = *dp = (png_byte)(((int)*rp - (int)*lp) & 0xff);

         sum += (v < 128) ? v : 256 - v;
      }
      if (sum < mins)
      {
         mins = sum;
         best_row = png_ptr->sub_row;
      }
   }

   /* up filter */
   if (png_ptr->do_filter & PNG_FILTER_UP)
   {
      png_bytep rp, dp, pp;
      png_uint_32 sum = 0;
      int i, v;

      for (i = 0, rp = row_buf + 1, dp = png_ptr->up_row + 1,
           pp = prev_row + 1; i < row_info->rowbytes; i++, rp++, pp++, dp++)
      {
         v = *dp = (png_byte)(((int)*rp - (int)*pp) & 0xff);

         sum += (v < 128) ? v : 256 - v;
      }
      if (sum < mins)
      {
         mins = sum;
         best_row = png_ptr->up_row;
      }
   }

   /* avg filter */
   if (png_ptr->do_filter & PNG_FILTER_AVG)
   {
      png_bytep rp, dp, pp, lp;
      png_uint_32 sum = 0;
      int i, v;

      for (i = 0, rp = row_buf + 1, dp = png_ptr->avg_row + 1,
           pp = prev_row + 1; i < bpp; i++, rp++, pp++, dp++)
      {
         v = *dp = (png_byte)(((int)*rp - ((int)*pp / 2)) & 0xff);

         sum += (v < 128) ? v : 256 - v;
      }
      for (lp = row_buf + 1; i < row_info->rowbytes;
           i++, rp++, pp++, lp++, dp++)
      {
         v = *dp = (png_byte)(((int)*rp - (((int)*pp + (int)*lp) / 2)) & 0xff);

         sum += (v < 128) ? v : 256 - v;
      }
      if (sum < mins)
      {
         mins = sum;
         best_row = png_ptr->avg_row;
      }
   }

   /* paeth filter */
   if (png_ptr->do_filter & PNG_FILTER_PAETH)
   {
      png_bytep rp, dp, pp, cp, lp;
      png_uint_32 sum = 0;
      int i, v;

      for (i = 0, rp = row_buf + 1, dp = png_ptr->paeth_row + 1,
           pp = prev_row + 1; i < bpp; i++, rp++, pp++, dp++)
      {
         v = *dp = (png_byte)(((int)*rp - (int)*pp) & 0xff);

         sum += (v < 128) ? v : 256 - v;
      }
      for (lp = row_buf + 1, cp = prev_row + 1; i < row_info->rowbytes;
           i++, rp++, pp++, lp++, dp++, cp++)
      {
         int a, b, c, pa, pb, pc, p;

         b = *pp;
         c = *cp;
         a = *lp;

         p = a + b - c;
         pa = abs(p - a);
         pb = abs(p - b);
         pc = abs(p - c);

         if (pa <= pb && pa <= pc)
            p = a;
         else if (pb <= pc)
            p = b;
         else
            p = c;

         v = *dp = (png_byte)(((int)*rp - p) & 0xff);

         sum += (v < 128) ? v : 256 - v;
      }
      if (sum < mins)
      {
         best_row = png_ptr->paeth_row;
      }
   }

   /* Do the actual writing of the filtered row data from the chosen filter */
   png_write_filtered_row(png_ptr, best_row);
}


/* do the actual writing of a filtered row */
void
png_write_filtered_row(png_structp png_ptr, png_bytep filtered_row)
{
   /* set up the zlib input buffer */
   png_ptr->zstream->next_in = filtered_row;
   png_ptr->zstream->avail_in = (uInt)png_ptr->row_info.rowbytes + 1;
   /* repeat until we have compressed all the data */
   do
   {
      int ret; /* return of zlib */

      /* compress the data */
      ret = deflate(png_ptr->zstream, Z_NO_FLUSH);
      /* check for compression errors */
      if (ret != Z_OK)
      {
         if (png_ptr->zstream->msg)
            png_error(png_ptr, png_ptr->zstream->msg);
         else
            png_error(png_ptr, "zlib error");
      }

      /* see if it is time to write another IDAT */
      if (!png_ptr->zstream->avail_out)
      {
         /* write the IDAT and reset the zlib output buffer */
         png_write_IDAT(png_ptr, png_ptr->zbuf, png_ptr->zbuf_size);
         png_ptr->zstream->next_out = png_ptr->zbuf;
         png_ptr->zstream->avail_out = (uInt)png_ptr->zbuf_size;
      }
   /* repeat until all data has been compressed */
   } while (png_ptr->zstream->avail_in);

   /* swap the current and previous rows */
   if (png_ptr->prev_row)
   {
      png_bytep tptr;

      tptr = png_ptr->prev_row;
      png_ptr->prev_row = png_ptr->row_buf;
      png_ptr->row_buf = tptr;
   }

   /* finish row - updates counters and flushes zlib if last row */
   png_write_finish_row(png_ptr);

#if defined(PNG_WRITE_FLUSH_SUPPORTED)
   png_ptr->flush_rows++;

   if (png_ptr->flush_dist > 0 &&
       png_ptr->flush_rows >= png_ptr->flush_dist)
   {
      png_write_flush(png_ptr);
   }
#endif /* PNG_WRITE_FLUSH_SUPPORTED */
}

