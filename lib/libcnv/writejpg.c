/*
 * Copyright (C) 1994-1995, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 * This code was taken from an example file.
 *
 */

#include "xp_core.h" //used to make library compile faster on win32 do not ifdef this or it wont work

/*writejpg.c jpeg compression wrapper for jpeg compression utilities*/
#include <stdio.h>
#include <setjmp.h>/*for error handler*/
#include "xp_core.h"/*defines of int32 ect*/

#include "ntypes.h" /* for MWContext to include libcnv.h*/
#include "libcnv.h"
#include "writejpg.h"
#include "..\jpeg\jinclude.h"
#include "..\jpeg\jpeglib.h"
//#include "jerror.h"		/* get library error codes too */

/* incomming data= (RGB,RGB,RGB)*/

struct my_error_mgr {
  struct jpeg_error_mgr pub;	/* "public" fields */

  jmp_buf setjmp_buffer;	/* for return to caller */ /*must include setjmp*/
};

typedef struct my_error_mgr * my_error_ptr;


/*
 * Here's the routine that will replace the standard error_exit method:
 */

void
my_error_exit (j_common_ptr cinfo)
{
  /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
  my_error_ptr myerr = (my_error_ptr) cinfo->err;

  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  (*cinfo->err->output_message) (cinfo);
  /* Return control to the setjmp point */
  longjmp(myerr->setjmp_buffer, 1);
}

/*given an array of pixels, writes a jpeg file*/
CONVERT_IMAGERESULT
write_JPEG_file (CONVERT_IMG_ARRAY p_rowarray,CONVERT_IMGCONTEXT *output,CONVERT_IMG_INFO *p_imageinfo,CONVERT_CALLBACKS p_callbacks)
{
  struct jpeg_compress_struct cinfo;
  struct my_error_mgr jerr;
  int16 row_stride;		/* physical row width in image buffer */

  cinfo.err = jpeg_std_error((struct jpeg_error_mgr *)&jerr);

  jerr.pub.error_exit = my_error_exit;
  jerr.pub.output_message=p_callbacks.m_displaybuffercallback;
  /* Establish the setjmp return context for my_error_exit to use. */
  if (setjmp(jerr.setjmp_buffer)) {
  /* If we get here, the JPEG code has signaled an error.
  * We need to clean up the JPEG object, close the input file, and return.
    */
    jpeg_close_file(&cinfo);
    jpeg_destroy_compress(&cinfo);
    return CONVERR_JPEGERROR;
  }
  /* Now we can initialize the JPEG compression object. */
  jpeg_create_compress(&cinfo);

  /* Here we use the library-supplied code to send compressed data to a
  * stdio stream.*/
  jpeg_file_dest(&cinfo, output->m_filename);

  cinfo.image_width = p_imageinfo->m_image_width; 	/* image width and height, in pixels */
  cinfo.image_height = p_imageinfo->m_image_height;
  cinfo.input_components = 3;		/* # of color components per pixel */
  cinfo.in_color_space = JCS_RGB; 	/* colorspace of input image */
                                    /* Now use the library's routine to set default compression parameters.
                                    * (You must set at least cinfo.in_color_space before calling this,
                                    * since the defaults depend on the source color space.)
  */
  jpeg_set_defaults(&cinfo);
  /* Now you can set any non-default parameters you wish to.
  * Here we just illustrate the use of quality (quantization table) scaling:
  */
  jpeg_set_quality(&cinfo, output->m_quality, TRUE /* limit to baseline-JPEG values */);

  /* Step 4: Start compressor */

  /* TRUE ensures that we will write a complete interchange-JPEG file.
  * Pass TRUE unless you are very sure of what you're doing.
  */
  jpeg_start_compress(&cinfo, TRUE);

  /* Step 5: while (scan lines remain to be written) */
  /*           jpeg_write_scanlines(...); */

  /* Here we use the library's state variable cinfo.next_scanline as the
  * loop counter, so that we don't have to keep track ourselves.
  * To keep things simple, we pass one scanline per call; you can pass
  * more if you wish, though.
  */
  row_stride = p_imageinfo->m_image_width * 3;	/* JSAMPLEs per row in image_buffer */

  while (cinfo.next_scanline < cinfo.image_height) 
    (void) jpeg_write_scanlines(&cinfo, (JSAMPARRAY)p_rowarray, cinfo.image_height);

  /* Step 6: Finish compression */

  jpeg_finish_compress(&cinfo);

  /* Step 7: release JPEG compression object */
  /* This is an important step since it will release a good deal of memory. */
  jpeg_destroy_compress(&cinfo);
  return CONV_OK;
}

