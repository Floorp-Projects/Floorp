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


#include "xp_core.h" //used to make library compile faster on win32 do not ifdef this or it wont work

#include <memory.h>
#include <stdio.h>
#include <malloc.h>

#include "xp_core.h"/*defines of int32 ect*/
#include "xpassert.h"
#include "xp_mcom.h"/*XP_STRDUP command*/
#include "ntypes.h" /*for mwcontext for callback routines*/

#include "libcnv.h"
#include "readbmp.h"
#include "writejpg.h"
//#include "writepng.h"
/*the following 4 includes are for the color quantization wrapper to colorqnt.h*/
#include "pbmplus.h"
#include "ppm.h"
#include "ppmcmap.h"
#include "colorqnt.h"



/*accepts a stream of bytes that will be read in and converted to a JPEG/GIF FILE
jpeg/gif file (default to p_output default) stream that is returned 
: you are responsible for free'ing the returned memory*/
typedef void * LPVOID;

CONVERT_IMG_INFO *create_img_info();
void copy_img_info(CONVERT_IMG_INFO *p_dest,CONVERT_IMG_INFO *p_origin);
void destroy_img_info(CONVERT_IMG_INFO *p_info);

void clean_convert_img_array(CONVERT_IMG_ARRAY array,int16 count);

/*PUBLIC API*/
convimgenum
select_file_type (CONVERT_IMGCONTEXT * p_input)
{
  int16 c;
  if ((c = getc(p_input->m_stream.m_file)) == EOF)
    return conv_unknown;
  if (ungetc(c, p_input->m_stream.m_file) == EOF)
    return conv_unknown;

  switch (c) {
  case 'B':
    p_input->m_imagetype=conv_bmp;
    return conv_bmp;
  default:
    p_input->m_imagetype=conv_unknown; 
    return conv_unknown;
    break;
  }

  return conv_unknown;			/* suppress compiler warnings */
}

/*PUBLIC API*/
CONVERT_IMAGERESULT 
convert_quantize_pixels(CONVERT_IMG_ARRAY imagearray,int16 imagewidth,int16 imageheight,int16 maxcolorvalue)
{
  /*wrapper for colorqnt.cpp we just want to change the pixels, we dont need any info*/
  CONVERT_IMAGERESULT t_result;
  int16 t_dummycolors;
  colorhist_vector t_dummyvector;
  t_result=quantize_colors(imagearray,imagewidth,imageheight,&maxcolorvalue,&t_dummycolors,&t_dummyvector);
  if ( t_dummyvector == (colorhist_vector) 0 )
    return t_result;
  ppm_freecolorhist( t_dummyvector );
  return t_result;
}

CONVERT_IMG_INFO *create_img_info()
{
  return calloc (1,sizeof(CONVERT_IMG_INFO));
}

void destroy_img_info(CONVERT_IMG_INFO *p_info)
{
  if (p_info)
  {
    if (p_info->m_colormap)
      free(p_info->m_colormap);
    free (p_info);
  }
}

void copy_img_info(CONVERT_IMG_INFO *p_dest,CONVERT_IMG_INFO *p_origin)
{
  XP_ASSERT(p_dest);
  XP_ASSERT(p_origin);
  memcpy(p_dest,p_origin,sizeof(CONVERT_IMG_INFO));/*shallow copy!!*/
}

/*PUBLIC API*/
CONVERT_IMAGERESULT convert_stream2image(CONVERT_IMGCONTEXT p_input,CONVERT_IMG_INFO *p_imageinfo,int16 p_numoutputs,char **p_outputfilenames) /*0== OK <0 == user cancel >0 == error*/
{
  CONVERT_IMGCONTEXT *t_outputparams;
  CONVERT_IMG_INFO *t_imageinfo=NULL;
  CONVERT_IMG_ARRAY t_rowarray=NULL;
  convimgenum t_outputtype;
  CONVERT_IMAGERESULT t_err;
  int16 i;
  XP_ASSERT(p_imageinfo);
  if ((!p_numoutputs)||(!p_imageinfo))
    return CONVERR_INVALIDDEST;
  if ((conv_unknown==p_input.m_imagetype)&&(CONVERT_MEMORY==p_input.m_stream.m_type)) /*dont know what it is and its not a file!*/
    return CONVERR_INVALIDSOURCE;/*reading unknown stream??*/
  t_outputparams=calloc(p_numoutputs,sizeof(CONVERT_IMGCONTEXT));
  if (!t_outputparams)
    return CONVERR_OUTOFMEMORY;
  else if ((conv_unknown==p_input.m_imagetype)&&(CONVERT_FILE==p_input.m_stream.m_type))
  {
    /*read in header info*/
    if (conv_unknown==select_file_type(&p_input))
      return CONVERR_INVALIDSOURCE;/*read file header and tell me what kind it is. also fill info into p_input*/
  }
  t_imageinfo=create_img_info();
  if (conv_bmp==p_input.m_imagetype) /*bitmap*/
  {
    t_err=start_input_bmp(t_imageinfo,&p_input);
    if (t_err==CONV_OK)
    {
#if 0
      if (t_imageinfo->m_bitsperpixel<=8)/*256Colors GIF  ..check colorspace*/
        t_outputtype=conv_png;
      else /* >8 means use JPEG*/
#endif /*no png support for composer yet*/
        t_outputtype=conv_jpeg;
      for (i=0;i<p_numoutputs;i++)/*need to set defaults for outputs*/
      {
        t_outputparams[i].m_imagetype=t_outputtype;
        t_outputparams[i].m_callbacks=p_input.m_callbacks;
      }
      t_err==finish_input_bmp(t_imageinfo,&p_input,&t_rowarray);
      if (t_err!=CONV_OK)
      {
        clean_convert_img_array(t_rowarray,t_imageinfo->m_image_height);
        destroy_img_info(t_imageinfo);
        return t_err;
      }
    }
    else
    {
      destroy_img_info(t_imageinfo);
      return t_err;
    }
  }
  /*PICT CRAP HERE AS WELL AS XPM*/
  else
  {
    destroy_img_info(t_imageinfo);
    return CONVERR_INVALIDSOURCE;
  }
  /*callback: this must give us the information on how to convert the image. in case of JPEG, give us the same as parse_switches in the JPEG libraries
  they can change t_defaultoutput if they must and change t_imageinfo for default values i.e. color quantization*/
  if (!p_input.m_callbacks.m_dialogimagecallback)
  {
    clean_convert_img_array(t_rowarray,t_imageinfo->m_image_height);
    destroy_img_info(t_imageinfo);
    return CONV_CANCEL;
  }
  t_err=(*p_input.m_callbacks.m_dialogimagecallback)(&p_input,t_outputparams,t_imageinfo,p_numoutputs,t_rowarray);
  if (CONV_OK!=t_err)
  {
    clean_convert_img_array(t_rowarray,t_imageinfo->m_image_height);
    destroy_img_info(t_imageinfo);
    return t_err;
  }
  for (i=0;i<p_numoutputs;i++)
  {
    if (t_outputparams[i].m_stream.m_type!=CONVERT_FILE)
    {
      /* And we're done! */
      clean_convert_img_array(t_rowarray,t_imageinfo->m_image_height);
      destroy_img_info(t_imageinfo);
      return t_err;
    }
    //copying strings to p_outputfilenames[i]
    if (p_outputfilenames)
      p_outputfilenames[i]=XP_STRDUP(t_outputparams[i].m_filename);
    switch(t_outputparams[i].m_imagetype)
    {
    case conv_jpeg:
      {
        t_err=write_JPEG_file(t_rowarray,&t_outputparams[i],t_imageinfo,p_input.m_callbacks);
        if (t_err!=CONV_OK)
        {
          /* And we're done! */
          clean_convert_img_array(t_rowarray,t_imageinfo->m_image_height);
          destroy_img_info(t_imageinfo);
          return t_err;
        }
      }
      break;
#if 0
    case conv_png:
      {
        /*color quantization*/
        t_err=write_PNG_file(t_rowarray,&(t_outputparams[i]),t_imageinfo,p_input.m_callbacks);
        if (t_err!=CONV_OK)
        {
          /* And we're done! */
          clean_convert_img_array(t_rowarray,t_imageinfo->m_image_height);
          destroy_img_info(t_imageinfo);
          return t_err;
        }
      }
      break;
#endif //no png support for composer yet
    case conv_plugin: /*plugin*/
      {
        /* to be safe we will clear colormap out out p_outputinfos*/
        clean_convert_img_array(t_rowarray,t_imageinfo->m_image_height);
        copy_img_info(p_imageinfo,t_imageinfo);/*shallow copy! no colormap*/
        destroy_img_info(t_imageinfo);/*the color map is destroyed here */
        return CONV_OK; /*get rid of compiler warnings*/
      }
      break;
    default:
      {
        /* to be safe we will clear colormap out out p_outputinfos*/
        clean_convert_img_array(t_rowarray,t_imageinfo->m_image_height);
        copy_img_info(p_imageinfo,t_imageinfo);/*shallow copy! no colormap*/
        destroy_img_info(t_imageinfo);/*the color map is destroyed here */
        return CONVERR_INVALIDDEST;
      }
      break;
    }
  }
  /* to be safe we will clear colormap out out p_outputinfos*/
  clean_convert_img_array(t_rowarray,t_imageinfo->m_image_height);
  copy_img_info(p_imageinfo,t_imageinfo);/*shallow copy! no colormap*/
  destroy_img_info(t_imageinfo);/*the color map is destroyed here */
  if (p_input.m_callbacks.m_completecallback) //its OK to be null
    t_err=(*p_input.m_callbacks.m_completecallback)(t_outputparams,p_numoutputs,p_input.m_pMWContext);
  else 
    t_err=CONV_OK;
  return t_err;
}



/*
this clean function assumes the array has been cleared to 0 before being used.
*/
void clean_convert_img_array(CONVERT_IMG_ARRAY array,int16 count)
{
  int16 i;
  if (!array)
    return;
  for (i=0;i<count;i++)
    if (array[i])
    free(array[i]);
  free(array);
}



int16 read_mem_stream(CONVERT_IMG_STREAM *p_stream,void *p_dest,uint16 p_bytecount)
{
  XP_ASSERT(p_stream);
  if ((p_stream->m_streamsize==0) ||((p_stream->m_currentindex+(DWORD)p_bytecount)<p_stream->m_streamsize))
  {
    memcpy(p_dest,p_stream->m_mem+p_stream->m_currentindex,p_bytecount);
    p_stream->m_currentindex+=p_bytecount;
    return p_bytecount;
  }
  return 0;/*read past end of stream*/
}

BYTE read_mem_stream_byte(CONVERT_IMG_STREAM *p_stream)
{
  XP_ASSERT(p_stream);
  if ((p_stream->m_streamsize==0) ||((p_stream->m_currentindex+1)<p_stream->m_streamsize))
  {
    return p_stream->m_mem[p_stream->m_currentindex++];
  }
  XP_ASSERT(0);
  return 0;/*read past end of stream*/
}

int16 read_param(CONVERT_IMG_STREAM *p_input,void *p_dest,uint16 p_bytecount)
{
  XP_ASSERT(p_input);
  if (CONVERT_MEMORY==p_input->m_type)
    return read_mem_stream(p_input,p_dest,p_bytecount);
  else if (CONVERT_FILE==p_input->m_type)
    return ReadOK(p_input->m_file,p_dest,p_bytecount);
  else
  {
    XP_ASSERT(0);
    return -1;
  }
}

BYTE read_param_byte(CONVERT_IMG_STREAM *p_input)
{
  XP_ASSERT(p_input);
  if (CONVERT_MEMORY==p_input->m_type)
    return read_mem_stream_byte(p_input);
  else if (CONVERT_FILE==p_input->m_type)
    return (BYTE) getc(p_input->m_file); 
  else
  {
    XP_ASSERT(0);
    return 0;
  }
}


