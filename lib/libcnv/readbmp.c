/*
 * readbmp.c
This file was taken from the Independent JPEG Group and modified extensively
 *
 * Copyright (C) 1994-1995, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
/
 /* This code is for reading bmps and translating them to RGB.  There are additions that Netscape has 
  * added to read more formats.
 */
#include "xp_core.h" //used to make library compile faster on win32 do not ifdef this or it wont work

/* readbmp : reads bmp and returns colormap (if any), CMN_IMAGEINFO,bits*/
#include <memory.h>
#include <stdio.h>
#include <malloc.h>
#include "xp_core.h"/*defines of int32 ect*/
#include "xpassert.h"
#include "ntypes.h" /* for MWContext to include libcnv.h*/

#include "libcnv.h"
#include "readbmp.h"

typedef unsigned char U_CHAR;
#define UCH(x)	((int16) (x))
typedef uint16 UINT16;

#define TRUE !FALSE

XP_Bool get_1bit_row_bmp (CONVERT_IMGCONTEXT *p_params,CONVERT_IMG_INFO *p_info,CONVERT_IMG_ROW p_outputbuffer);
XP_Bool get_4bit_row_bmp (CONVERT_IMGCONTEXT *p_params,CONVERT_IMG_INFO *p_info,CONVERT_IMG_ROW p_outputbuffer);
XP_Bool get_8bit_row_bmp (CONVERT_IMGCONTEXT *p_params,CONVERT_IMG_INFO *p_info,CONVERT_IMG_ROW p_outputbuffer);
XP_Bool get_24bit_row_bmp (CONVERT_IMGCONTEXT *p_params,CONVERT_IMG_INFO *p_info,CONVERT_IMG_ROW p_outputbuffer);
XP_Bool get_16bit_row_bmp (CONVERT_IMGCONTEXT *p_params,CONVERT_IMG_INFO *p_info,CONVERT_IMG_ROW p_outputbuffer);
XP_Bool get_32bit_row_bmp (CONVERT_IMGCONTEXT *p_params,CONVERT_IMG_INFO *p_info,CONVERT_IMG_ROW p_outputbuffer);

void process_color_mask(uint32 *p_mask,char *p_shift,uint16 *p_pad);

CONVERT_IMAGERESULT read_colormap_bmp (CONVERT_IMGCONTEXT *p_inputparam, BYTE *p_colormap,int16 cmaplen, int16 mapentrysize);

///////////////////////////////////////
//////////READ BMP/////////////////////
CONVERT_IMAGERESULT
start_input_bmp (CONVERT_IMG_INFO *p_imageinfo, CONVERT_IMGCONTEXT *p_inputparam)
{
  CONVERT_IMAGERESULT t_err;
  U_CHAR bmpfileheader[14];
  U_CHAR bmpinfoheader[64];
#define GET_2B(array,offset)  ((uint16) UCH(array[offset]) + \
			       (((uint16) UCH(array[offset+1])) << 8))
#define GET_4B(array,offset)  ((int32) UCH(array[offset]) + \
			       (((int32) UCH(array[offset+1])) << 8) + \
			       (((int32) UCH(array[offset+2])) << 16) + \
			       (((int32) UCH(array[offset+3])) << 24))
  int32 bfOffBits;
  int32 headerSize;
  int32 biWidth = 0;		/* initialize to avoid compiler warning */
  int32 biHeight = 0;
  uint16 biPlanes;
  int32 biCompression;
  int32 biXPelsPerMeter,biYPelsPerMeter;
  int32 biClrUsed = 0;
  int16 mapentrysize = 0;		/* 0 indicates no colormap */
  DWORD t_index=0;/*index to stream*/
  int32 bPad;
  uint16 row_width;
  if (p_inputparam->m_stream.m_type==CONVERT_FILE)
  {
	  if (!read_param(&p_inputparam->m_stream,bmpfileheader,14))
	  {/*cant read fileheader*/
		return CONVERR_INVALIDFILEHEADER;
	  }
	  p_imageinfo->m_image_size+=14;
	  /* Read and verify the bitmap file header */
	  if (GET_2B(bmpfileheader,0) != 0x4D42) /* 'BM' */
		return CONVERR_INVALIDFILEHEADER;
	  bfOffBits = (int32) GET_4B(bmpfileheader,10);
  }
  if (! read_param(&p_inputparam->m_stream, bmpinfoheader, 4))
	return CONVERR_INVALIDIMAGEHEADER;//cant read 4 bytes
  p_imageinfo->m_image_size+=4;
  headerSize = (int32) GET_4B(bmpinfoheader,0);
  if (headerSize < 12 || headerSize > 64)
	return CONVERR_INVALIDIMAGEHEADER;
  if (! read_param(
      &p_inputparam->m_stream, 
      bmpinfoheader+4,
      (int16)(headerSize-4))) /*casting here is ok beacause of check for >64 above*/
	return CONVERR_INVALIDIMAGEHEADER;
  p_imageinfo->m_image_size+=headerSize-4;
  switch ((int16) headerSize) {
  case 12:
	/* Decode OS/2 1.x header (Microsoft calls this a BITMAPCOREHEADER) */
	biWidth = (int32) GET_2B(bmpinfoheader,4);
	biHeight = (int32) GET_2B(bmpinfoheader,6);
	biPlanes = GET_2B(bmpinfoheader,8);
	p_imageinfo->m_bitsperpixel = (int16) GET_2B(bmpinfoheader,10);

	switch (p_imageinfo->m_bitsperpixel) {
	case 8:			/* colormapped image */
	  mapentrysize = 3;		/* OS/2 uses RGBTRIPLE colormap */
	  break;
	case 16:
    case 24:			/* RGB image */
	  break;
	default:
	  return CONVERR_INVALIDBITDEPTH;
	  break;
	}
	if (biPlanes != 1)
	  return CONVERR_INVALIDBITDEPTH;
	break;
  case 40:
  case 64:
	/* Decode Windows 3.x header (Microsoft calls this a BITMAPINFOHEADER) */
	/* or OS/2 2.x header, which has additional fields that we ignore */
	biWidth = GET_4B(bmpinfoheader,4);
	biHeight = GET_4B(bmpinfoheader,8);
	biPlanes = GET_2B(bmpinfoheader,12);
	p_imageinfo->m_bitsperpixel = (int16) GET_2B(bmpinfoheader,14);
	biCompression = GET_4B(bmpinfoheader,16);
	biXPelsPerMeter = GET_4B(bmpinfoheader,24);
	biYPelsPerMeter = GET_4B(bmpinfoheader,28);
	biClrUsed = GET_4B(bmpinfoheader,32);
	/* biSizeImage, biClrImportant fields are ignored */
	switch (p_imageinfo->m_bitsperpixel) {
	case 1:
		if (!biClrUsed)/* 0 denotes maximum usage of colors*/
			biClrUsed=2;
	case 4:
		if (!biClrUsed)/* 0 denotes maximum usage of colors*/
			biClrUsed=16;
	case 8:			/* colormapped images */
		if (!biClrUsed)/* 0 denotes maximum usage of colors*/
			biClrUsed=256;
        mapentrysize = 4;		/* Windows uses RGBQUAD colormap */
		break;
	case 24:			/* RGB image */
	  break;
    case 16:
	case 32:			/* RGB image with masks*/
        if (biCompression==3) /*BI_BITFIELDS*/
        {
            biClrUsed=3; /*need to get masks for RGB*/
            mapentrysize = 4;		/* Windows uses RGBQUAD colormap */
        }
	  break;
	default:
	  return CONVERR_INVALIDBITDEPTH;
	  break;
	}

	if (biPlanes != 1)
	  return FALSE;
	if (biCompression != 0)
        if (3!=biCompression)
	        return CONVERR_COMPRESSED;//cant handle compressed bitmaps

	if (biXPelsPerMeter > 0 && biYPelsPerMeter > 0) {
	  /* Set JFIF density parameters from the BMP data */
	  p_imageinfo->m_X_density = (UINT16) (biXPelsPerMeter/100); /* 100 cm per meter */
	  p_imageinfo->m_Y_density = (UINT16) (biYPelsPerMeter/100);
	  p_imageinfo->m_density_unit = 2;	/* dots/cm */
	}
	break;
  default:
	return CONVERR_INVALIDIMAGEHEADER;
	break;
  }

  /* Compute distance to bitmap data --- will adjust for colormap below */
  bPad = bfOffBits - (headerSize + 14);

  /* Read the colormap, if any */
  if (mapentrysize > 0) {
	if (biClrUsed <= 0)
	  biClrUsed = 256;		/* assume it's 256 */
	else if (biClrUsed > 256)
	  return CONVERR_INVALIDCOLORMAP;
	/* Allocate space to store the colormap */
	p_imageinfo->m_colormap = (BYTE *)malloc(biClrUsed*3);
	/* and read it from the file */
	t_err=read_colormap_bmp(p_inputparam,p_imageinfo->m_colormap, (int16) biClrUsed, mapentrysize);
	if (t_err!=CONV_OK)
		return t_err;
	p_imageinfo->m_image_size+=biClrUsed*mapentrysize;/*mapentrysize for windows bitmaps*/ 
	/* account for size of colormap */
	bPad -= biClrUsed * mapentrysize;
  }

  if (CONVERT_FILE==p_inputparam->m_stream.m_type)/* Skip any remaining pad bytes */
  {
	if (bPad < 0)			/* incorrect bfOffBits value? */
		return CONVERR_INVALIDIMAGEHEADER;
	while (--bPad >= 0) 
	{
		(void) read_param_byte(&p_inputparam->m_stream);
		p_imageinfo->m_image_size++;
	}
  }
  /* Compute row width in file, including padding to 4-byte boundary */
  switch (p_imageinfo->m_bitsperpixel)
  {
  case 1:
	  row_width = biWidth/8;
	  if (biWidth&7)
		  row_width++;
	  break;
  case 4:
	  row_width = biWidth/2;
	  if (biWidth&1)
		  row_width++;
	  break;
  case 8:
	row_width = (uint16) biWidth;
	break;
  case 16:
	row_width = (uint16) (biWidth * 2);
	break;
  case 24:
	row_width = (uint16) (biWidth * 3);
	break;
  case 32:
	row_width = (uint16) (biWidth * 4);
	break;
  default:
	  return CONVERR_INVALIDBITDEPTH;
  }
  p_imageinfo->m_stride=row_width;
  while ((row_width & 3) != 0) 
	row_width++;
  p_imageinfo->m_row_width = row_width;
  p_imageinfo->m_stride=p_imageinfo->m_row_width-p_imageinfo->m_stride;//difference

  /* p_imageinfo->m_in_color_space = JCS_RGB;*/ 
  /* in color space is ignored. was only for JPEG PAL format*/
  p_imageinfo->m_input_components = 3;
  p_imageinfo->m_data_precision = 8;
  p_imageinfo->m_image_width = (uint16) biWidth;
  p_imageinfo->m_image_height = (uint16) biHeight;
  p_imageinfo->m_numcolorentries=(short)biClrUsed;
  return CONV_OK;
}
///////////////////////////////////
///////END START READ BMP//////////
///////////////////////////////////

///////////////////////////////////
///////COLOR MAP READ//////////////
///////////////////////////////////

CONVERT_IMAGERESULT
read_colormap_bmp (CONVERT_IMGCONTEXT *p_inputparam, BYTE *p_colormap,int16 cmaplen, int16 mapentrysize)
/* Read the colormap from a BMP file */
{
  int16 i;
  
  switch (mapentrysize) {
  case 3:
    /* BGR format (occurs in OS/2 files) */
    for (i = 0; i < cmaplen; i++) {
      p_colormap[i*3+2]=read_param_byte(&p_inputparam->m_stream);//file or mem doesnt matter 
      p_colormap[i*3+1]=read_param_byte(&p_inputparam->m_stream);//file or mem doesnt matter 
      p_colormap[i*3+0]=read_param_byte(&p_inputparam->m_stream);//file or mem doesnt matter 
    }
    break;
  case 4:
    /* BGR0 format (occurs in MS Windows files) */
    for (i = 0; i < cmaplen; i++) {
      p_colormap[i*3+2]=read_param_byte(&p_inputparam->m_stream);//file or mem doesnt matter 
      p_colormap[i*3+1]=read_param_byte(&p_inputparam->m_stream);//file or mem doesnt matter 
      p_colormap[i*3+0]=read_param_byte(&p_inputparam->m_stream);//file or mem doesnt matter 
      (void) read_param_byte(&p_inputparam->m_stream);
    }
    break;
  default:
    XP_ASSERT(FALSE);
    return CONVERR_INVALIDCOLORMAP;
    break;
  }
  return CONV_OK;
}

///////////////////////////////END COLORMAP READ




CONVERT_IMAGERESULT
finish_input_bmp(CONVERT_IMG_INFO *p_imageinfo, CONVERT_IMGCONTEXT *p_input,CONVERT_IMG_ARRAY *p_returnarray)
{
  int16 i;
  CONVERT_IMG_ARRAY t_returnarray;
  int16 t_absheight=p_imageinfo->m_image_height;
  int16 t_start,t_end;
  int16 t_direction;
  if (t_absheight<0) /* negative height == top down */
  {
    t_absheight*= -1;
    t_start=0;
    t_end=t_absheight;
    t_direction= 1;
  }
  else
  {
    t_start =t_absheight-1;
    t_end= -1;
    t_direction= -1;
  }
  t_returnarray=(CONVERT_IMG_ARRAY)malloc(t_absheight*sizeof (CONVERT_IMG_ROW));
  if (!t_returnarray) /*out of memory*/
    return CONVERR_OUTOFMEMORY;
  *p_returnarray=t_returnarray;
  memset(t_returnarray,0,t_absheight*sizeof (CONVERT_IMG_ROW *));
  /* want to initialize the memory to 0 so in case of error we can free each row.*/

  XP_ASSERT(p_imageinfo);
  XP_ASSERT(p_input);
  for (i=t_start;i!=t_end;i+=t_direction)
  {
    t_returnarray[i]=(CONVERT_IMG_ROW)malloc(p_imageinfo->m_image_width*3); /* *3 because RGB */;	/* pointer to JSAMPLE row[s] */
    if (!t_returnarray[i])
    {
      return CONVERR_BADREAD;
    }
    switch (p_imageinfo->m_bitsperpixel)
    {
    case 1:
      if (!get_1bit_row_bmp(p_input,p_imageinfo,t_returnarray[i]))/*true if succeeded*/
      {
        return CONVERR_BADREAD; 
      }
      break;
    case 4:
      if (!get_4bit_row_bmp(p_input,p_imageinfo,t_returnarray[i]))/*true if succeeded*/
      {
        return CONVERR_BADREAD; 
      }
      break;
    case 8:
      if (!get_8bit_row_bmp(p_input,p_imageinfo,t_returnarray[i]))/*true if succeeded*/
      {
        return CONVERR_BADREAD; 
      }
      break;
    case 16:
      if (!get_16bit_row_bmp(p_input,p_imageinfo,t_returnarray[i]))/*true if succeeded*/
      {
        return CONVERR_BADREAD; 
      }
      break;
    case 24:
      if (!get_24bit_row_bmp(p_input,p_imageinfo,t_returnarray[i]))/*true if succeeded*/
      {
        return CONVERR_BADREAD; 
      }
      break;
    case 32:
      if (!get_32bit_row_bmp(p_input,p_imageinfo,t_returnarray[i]))/*true if succeeded*/
      {
        return CONVERR_BADREAD; 
      }
      break;
    default:
      {
        XP_ASSERT(FALSE); /* what the heck happened!*/
        return CONVERR_INVALIDBITDEPTH; 
      }
    }  
  }
  return CONV_OK;
}



XP_Bool
get_8bit_row_bmp (CONVERT_IMGCONTEXT *p_params,CONVERT_IMG_INFO *p_info,CONVERT_IMG_ROW p_outputbuffer)
/* This version is for reading 8-bit colormap indexes */
{
  CONVERT_IMG_ROW outptr = p_outputbuffer;
  BYTE t_byte;
  int16 col;
  if (!p_info->m_colormap||!p_outputbuffer||!p_params||!p_info)
  {
    XP_ASSERT(FALSE);
    return FALSE;
  }
  for (col = p_info->m_image_width; col > 0; col--) 
  {
    t_byte = read_param_byte(&p_params->m_stream);
    p_info->m_image_size++;
    if (EOF==t_byte)
      return FALSE;
    if (t_byte>p_info->m_numcolorentries)/*bad index*/
      return FALSE;
    *outptr++ = p_info->m_colormap[0+t_byte*3];	/* can omit GETJSAMPLE() safely */
    if (EOF==t_byte)
      return FALSE;
    *outptr++ = p_info->m_colormap[1+t_byte*3];
    if (EOF==t_byte)
      return FALSE;
    *outptr++ = p_info->m_colormap[2+t_byte*3];
  }
  for (col=0;col<p_info->m_stride;col++) /*read to 4byte multiple*/
  {
    t_byte = read_param_byte(&p_params->m_stream);
    p_info->m_image_size++;
    if (EOF==t_byte)
      return FALSE;
  }
  return TRUE;
}

void process_color_mask(uint32 *p_mask,char *p_shift,uint16 *p_pad)
{
  uint32 t_mask=*p_mask;
  *p_shift=0;
  *p_pad=0;
  if (!p_mask||!p_shift||!p_pad)
  {
    XP_ASSERT(FALSE);
    return;
  }
  while (!(t_mask&1))
  {
    (*p_shift)+=1;
    t_mask=t_mask>>1;
  }
  while (t_mask&256) /*larger than 8bp color key i.e. 10-3-3*/
    (*p_shift)+=1;
  while (t_mask<128)/* dont know where the mask is that is why we dont put this in the same loop*/
  {
    (*p_shift)--;
    t_mask=t_mask<<1;
    (*p_pad)=(*p_pad)<<1;
    (*p_pad)++;
  }
}



XP_Bool
get_16bit_row_bmp (CONVERT_IMGCONTEXT *p_params,CONVERT_IMG_INFO *p_info,CONVERT_IMG_ROW p_outputbuffer)
/* This version is for reading 8-bit colormap indexes */
{

  CONVERT_IMG_ROW outptr = p_outputbuffer;
  BYTE t_byte;
  int16 col;
  uint32 t_redmask=0x00007C00;
  uint32 t_greenmask=0x000003E0;
  uint32 t_bluemask=0x0000001F;
  char t_redshift=10;
  char t_greenshift=5;
  char t_blueshift=0;
  uint16 t_redpad=0;
  uint16 t_greenpad=0;
  uint16 t_bluepad=0;

  if (!p_outputbuffer||!p_params||!p_info)
  {
    XP_ASSERT(FALSE);
    return FALSE;
  }
  if (p_info->m_numcolorentries==3)
  {
    if (!p_info->m_colormap)
    {
      XP_ASSERT(FALSE);
      return FALSE;
    }
    t_redmask=(p_info->m_colormap[0]<<16)+(p_info->m_colormap[1]<<8)+p_info->m_colormap[2];
    t_greenmask=(p_info->m_colormap[3]<<16)+(p_info->m_colormap[4]<<8)+p_info->m_colormap[5];
    t_bluemask=(p_info->m_colormap[6]<<16)+(p_info->m_colormap[7]<<8)+p_info->m_colormap[8];
    process_color_mask(&t_redmask,&t_redshift,&t_redpad);
    process_color_mask(&t_greenmask,&t_greenshift,&t_greenpad);
    process_color_mask(&t_bluemask,&t_blueshift,&t_bluepad);
  }
  for (col = p_info->m_image_width; col > 0; col--) 
  {
    uint16 t_pixel;/*16bits*/
    uint16 t_temppixel;
    if (-1==read_param(&p_params->m_stream,&t_pixel,sizeof(t_pixel)))
      return FALSE;
    p_info->m_image_size+=sizeof(t_pixel);
    t_temppixel=(uint16)(t_redmask&t_pixel);
    if (t_redshift>0)
      t_temppixel=t_temppixel>>t_redshift;
    else
      t_temppixel=t_temppixel<<(-1 *t_redshift);
    t_temppixel=t_temppixel+t_redpad;
    outptr[0]=(BYTE)t_temppixel;
    t_temppixel=(uint16)(t_greenmask&t_pixel);
    if (t_greenshift>0)
      t_temppixel=t_temppixel>>t_greenshift;
    else
      t_temppixel=t_temppixel<<(-1 *t_greenshift);
    t_temppixel=t_temppixel+t_greenpad;
    outptr[1]=(BYTE)t_temppixel;
    t_temppixel=(uint16)(t_bluemask&t_pixel);
    if (t_blueshift>0)
      t_temppixel=t_temppixel>>t_blueshift;
    else
      t_temppixel=t_temppixel<<(-1 *t_blueshift);
    t_temppixel=t_temppixel+t_bluepad;
    outptr[2]=(BYTE)t_temppixel;
    outptr+=3;
  }
  for (col=0;col<p_info->m_stride;col++) /*read to 4byte multiple*/
  {
    t_byte = read_param_byte(&p_params->m_stream);
    p_info->m_image_size++;
    if (EOF==t_byte)
      return FALSE;
  }
  return TRUE;
}



XP_Bool
get_32bit_row_bmp (CONVERT_IMGCONTEXT *p_params,CONVERT_IMG_INFO *p_info,CONVERT_IMG_ROW p_outputbuffer)
/* This version is for reading 8-bit colormap indexes */
{

  CONVERT_IMG_ROW outptr = p_outputbuffer;
  BYTE t_byte;
  int16 col;
  uint32 t_redmask=  0x00FF0000;/*RGB(8,8,8)*/
  uint32 t_greenmask=0x0000FF00;
  uint32 t_bluemask= 0x000000FF;
  BYTE t_redshift=16;
  BYTE t_greenshift=8;
  BYTE t_blueshift=0;
  uint16 t_redpad=0;
  uint16 t_greenpad=0;
  uint16 t_bluepad=0;
  if (!p_outputbuffer||!p_params||!p_info)
  {
    XP_ASSERT(FALSE);
    return FALSE;
  }
  if (p_info->m_numcolorentries==3)
  {
    if (!p_info->m_colormap)
    {
      XP_ASSERT(FALSE);
      return FALSE;
    }
    t_redmask=(p_info->m_colormap[0]<<16)+(p_info->m_colormap[1]<<8)+p_info->m_colormap[2];
    t_greenmask=(p_info->m_colormap[3]<<16)+(p_info->m_colormap[4]<<8)+p_info->m_colormap[5];
    t_bluemask=(p_info->m_colormap[6]<<16)+(p_info->m_colormap[7]<<8)+p_info->m_colormap[8];
    process_color_mask(&t_redmask,&t_redshift,&t_redpad);
    process_color_mask(&t_greenmask,&t_greenshift,&t_greenpad);
    process_color_mask(&t_bluemask,&t_blueshift,&t_bluepad);
  }

  for (col = p_info->m_image_width; col > 0; col--) 
  {
    uint32 t_pixel;
    if (-1==read_param(&p_params->m_stream,&t_pixel,sizeof(t_pixel)))
      return FALSE;
    p_info->m_image_size+=sizeof(t_pixel);
    if (t_redshift>0)
      outptr[0]=(BYTE)((t_redmask&t_pixel)>>t_redshift);
    else
      outptr[0]=(BYTE)((t_redmask&t_pixel)<<(-1*t_redshift));
    outptr[0]+=t_redpad;
    if (t_greenshift>0)
      outptr[1]=(BYTE)((t_greenmask&t_pixel)>>t_greenshift);
    else
      outptr[1]=(BYTE)((t_greenmask&t_pixel)<<(-1*t_greenshift));
    outptr[1]+=t_greenpad;
    if (t_blueshift>0)
      outptr[2]=(BYTE)((t_bluemask&t_pixel)>>t_blueshift);
    else
      outptr[2]=(BYTE)((t_bluemask&t_pixel)<<(-1*t_blueshift));
    outptr[2]+=t_bluepad;
    outptr+=3;
  }
  for (col=0;col<p_info->m_stride;col++) /*read to 4byte multiple*/
  {
    t_byte = read_param_byte(&p_params->m_stream);
    p_info->m_image_size++;
    if (EOF==t_byte)
      return FALSE;
  }
  return TRUE;
}



XP_Bool
get_4bit_row_bmp (CONVERT_IMGCONTEXT *p_params,CONVERT_IMG_INFO *p_info,CONVERT_IMG_ROW p_outputbuffer)
/* This version is for reading 8-bit colormap indexes */
{
  CONVERT_IMG_ROW outptr = p_outputbuffer;
  BYTE t_byte;
  BYTE t_value; /*4bitvalue for index into color table*/
  int16 col;
  if (!p_info->m_colormap||!p_outputbuffer||!p_params||!p_info)
  {
    XP_ASSERT(FALSE);
    return FALSE;
  }
  for (col = 0; col <p_info->m_image_width; col++) 
  {
    if (0==(col%2))
    {	t_byte = read_param_byte(&p_params->m_stream);
    p_info->m_image_size++;
    }
    t_value = (t_byte>>4)& 0x0F; /*high order 4 bits, shift right, mask high order crap*/
    if (EOF==t_byte)
      return FALSE;
    if (t_value>p_info->m_numcolorentries)/*bad index*/
      return FALSE;
    *outptr++ = p_info->m_colormap[0+t_value*3];	/* can omit GETJSAMPLE() safely */
    if (EOF==t_byte)
      return FALSE;
    *outptr++ = p_info->m_colormap[1+t_value*3];
    if (EOF==t_byte)
      return FALSE;
    *outptr++ = p_info->m_colormap[2+t_value*3];
    t_byte<<=4; /*next pixel*/
  }
  for (col=0;col<p_info->m_stride;col++) /*read to 4byte multiple*/
  {
    t_byte = read_param_byte(&p_params->m_stream);
    p_info->m_image_size++;
    if (EOF==t_byte)
      return FALSE;
  }
  return TRUE;
}



XP_Bool
get_1bit_row_bmp (CONVERT_IMGCONTEXT *p_params,CONVERT_IMG_INFO *p_info,CONVERT_IMG_ROW p_outputbuffer)
/* This version is for reading 8-bit colormap indexes */
{
  CONVERT_IMG_ROW outptr = p_outputbuffer;
  BYTE t_byte;
  int16 col,bit;
  if (!p_info->m_colormap||!p_outputbuffer||!p_params||!p_info)
  {
    XP_ASSERT(FALSE);
    return FALSE;
  }
  if (!p_outputbuffer)
    return FALSE;
  for (bit=0;bit<p_info->m_image_width;bit++)
  {
    if (0==(bit%8))
    {
      t_byte = read_param_byte(&p_params->m_stream);
      p_info->m_image_size++;
    }
    //start from left by & with 128 then shift one at a time
    if (t_byte&128)
    {
      *outptr++ = p_info->m_colormap[3];
      *outptr++ = p_info->m_colormap[4];
      *outptr++ = p_info->m_colormap[5];
    }
    else
    {
      *outptr++ = p_info->m_colormap[0];
      *outptr++ = p_info->m_colormap[1];
      *outptr++ = p_info->m_colormap[2];
    }
    t_byte<<=1;
  }
  for (col=0;col<p_info->m_stride;col++) /*read to 4byte multiple*/
  {
    t_byte = read_param_byte(&p_params->m_stream);
    p_info->m_image_size++;
    if (EOF==t_byte)
      return FALSE;
  }
  return TRUE;
}



XP_Bool
get_24bit_row_bmp (CONVERT_IMGCONTEXT *p_params,CONVERT_IMG_INFO *p_info,CONVERT_IMG_ROW p_outputbuffer)
/* This version is for reading 24-bit pixels */
{
  CONVERT_IMG_ROW outptr = p_outputbuffer;
  BYTE t_byte;
  int16 col;
  if (!p_outputbuffer||!p_params||!p_info)
  {
    XP_ASSERT(FALSE);
    return FALSE;
  }
  for (col = p_info->m_image_width; col > 0; col--) 
  {
    t_byte = read_param_byte(&p_params->m_stream);
    p_info->m_image_size++;
    if (EOF==t_byte)
      return FALSE;
    outptr[2] =t_byte;
    t_byte = read_param_byte(&p_params->m_stream);
    p_info->m_image_size++;
    if (EOF==t_byte)
      return FALSE;
    outptr[1] =t_byte;
    t_byte = read_param_byte(&p_params->m_stream);
    p_info->m_image_size++;
    if (EOF==t_byte)
      return FALSE;
    outptr[0] =t_byte;
    outptr+=3;/*next RGB*/
  }
  for (col=0;col<p_info->m_stride;col++) /*read to 4byte multiple*/
  {
    t_byte = read_param_byte(&p_params->m_stream);
    p_info->m_image_size++;
    if (EOF==t_byte)
      return FALSE;
  }
  return TRUE;
}




///////////////////////////////////////////////
////////////////////END READ///////////////////
///////////////////////////////////////////////
