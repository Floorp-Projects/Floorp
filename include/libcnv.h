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

//"libcnv.h"
#ifndef _LIBCNV_H
#define _LIBCNV_H
#ifndef _IMAGE_CONVERT
#error _IMAGE_CONVERT SYMBOL NEEDED BEFORE INCLUSION
#endif /*_IMAGE_CONVERT*/

#ifndef __cplusplus
typedef unsigned char BYTE;
typedef uint32 DWORD;
#endif

#include "xp_core.h"/*defines of int32 ect*/
#include "xp_file.h"
#include "xp_mem.h"/*XP_HUGE*/

#define MAXIMAGEPATHLEN 255



typedef enum
{
    CONV_CANCEL,
    CONV_OK,
    CONVERR_INVALIDSOURCE,
    CONVERR_INVALIDDEST,
    CONVERR_INVALIDFILEHEADER,
    CONVERR_INVALIDIMAGEHEADER,
    CONVERR_INVALIDBITDEPTH,
    CONVERR_INVALIDCOLORMAP,
    CONVERR_BADREAD,
    CONVERR_OUTOFMEMORY,
    CONVERR_JPEGERROR,
    CONVERR_COMPRESSED,
    CONVERR_BADPLANES,
    CONVERR_BADWRITE,
    CONVERR_INVALIDPARAMS,
    CONVERR_UNKNOWN,
    NUM_CONVERR
}CONVERT_IMAGERESULT;



typedef BYTE * CONVERT_IMG_ROW;
typedef CONVERT_IMG_ROW * CONVERT_IMG_ARRAY;



typedef enum
{
	conv_unknown,
	conv_png,
	conv_jpeg,
	conv_bmp,
	conv_pict,
	conv_xpm,
    conv_rgb,
    conv_plugin
}convimgenum;


typedef struct tagCONVERT_IMG_STREAM
{
	XP_HUGE_CHAR_PTR m_mem;
	FILE *m_file;/*used only with type 0 -must allready be opened for read or write does not use current index ect.*/
	int16 m_type;/*0=CONVERT_FILE 1=CONVERT_MEMORY*/
	DWORD m_streamsize;/* 0== unlimited */
	DWORD m_currentindex;
}CONVERT_IMG_STREAM;



/*Sent in a BITMAP structure*/
#define CONVERT_MEMORY 1
#define CONVERT_FILE 0


typedef struct tagCONVERT_IMGCONTEXT CONVERT_IMGCONTEXT;
typedef struct tagCONVERT_IMG_INFO CONVERT_IMG_INFO;

typedef CONVERT_IMAGERESULT (*CONVERT_DIALOGIMAGECALLBACK)(CONVERT_IMGCONTEXT *input,
                                             CONVERT_IMGCONTEXT *outputarray,
                                             CONVERT_IMG_INFO *imginfo,
                                             int16 numoutput,
                                             CONVERT_IMG_ARRAY imagearray);
typedef CONVERT_IMAGERESULT (*CONVERT_COMPLETECALLBACK)(CONVERT_IMGCONTEXT *outputarray,int16 p_numoutputs,void *hook);

typedef void (*CONVERT_BUFFERCALLBACK)(void *);/*j_common_ptr);*/



typedef struct tagCONVERT_CALLBACKS
{
    CONVERT_DIALOGIMAGECALLBACK m_dialogimagecallback;
    CONVERT_BUFFERCALLBACK m_displaybuffercallback;
    CONVERT_COMPLETECALLBACK m_completecallback;
}CONVERT_CALLBACKS;



typedef struct tagCONVERT_IMGCONTEXT
{
	convimgenum m_imagetype;
	CONVERT_IMG_STREAM m_stream;/*used with m_streamtype 1,2*/
    int16 m_quality;
    char m_filename[MAXIMAGEPATHLEN];/*will not be used to open FILE *. used for output. maybe in future will open file?*/
    CONVERT_CALLBACKS m_callbacks;
#ifdef XP_OS2
    XP_OS2_ARG(void *m_parentwindow);/*used for callbacks to bring up dialog boxes. void * = CWnd *for Windows*/
#else
    XP_WIN_ARG(void *m_parentwindow);/*used for callbacks to bring up dialog boxes. void * = CWnd *for Windows*/
#endif
    void *m_pMWContext;//used for callbacks to insert the image. and for plugins
}CONVERT_IMGCONTEXT;



typedef struct tagCONVERT_IMG_INFO
{
	BYTE *m_colormap;
	int16 m_numcolorentries;
	uint16 m_X_density;
	uint16 m_Y_density;
	int16 m_density_unit;
	int16 m_in_color_space;
	int16 m_input_components;
	int16 m_data_precision;
	int16 m_image_width;/*pixel width*/
	int16 m_image_height;/*pixel_height*/
	int16 m_bitsperpixel;
	int16 m_row_width;/*width in bytes*/
	int16 m_stride; /*row_width-(pixel_width*bpp)/8 */
	DWORD m_image_size; /*informational purposes*/
}CONVERT_IMG_INFO;



#ifdef __cplusplus
extern "C" 
{
#endif


    
/****************************/    
/*API CALLS AND DECLARATIONS*/
/****************************/    

/*converts input to p_numoutput many outputs*/
/*p_outpuffilenames must be a PREALLOCATED array of char *s at least p_numoutputs char *s  these pointers will
point to output filenames that YOU will be responsible to destroy!
    you may pass in null for p_outputfilenames and it wil*/
CONVERT_IMAGERESULT convert_stream2image(CONVERT_IMGCONTEXT p_input,CONVERT_IMG_INFO *p_imageinfo,int16 p_numoutputs,char **p_outputfilenames);

/*quantize_pixels will change the imagearray to have only maxcolors distinct values*/
CONVERT_IMAGERESULT convert_quantize_pixels(CONVERT_IMG_ARRAY imagearray,int16 imagewidth,int16 imageheight,int16 maxcolorvalue);

/*given a imagecontext, it will tell you if it is a png,bmp,gif ect*/
convimgenum select_file_type (CONVERT_IMGCONTEXT * p_input);

/****************************/    
/*END API CALLS AND DECLARATIONS*/
/****************************/    


/****************************/    
/*STREAM DECLARATIONS*/
/****************************/    

/*CONV_IMG_FREAD taken from JPEG libraries for independence from common header file*/
#define CONV_IMG_FREAD(file,buf,sizeofbuf)  \
  ((size_t) fread((void *) (buf), (size_t) 1, (size_t) (sizeofbuf), (file)))

#define	ReadOK(file,buffer,len)	(CONV_IMG_FREAD(file,buffer,len) == ((size_t) (len)))

int16 read_mem_stream(CONVERT_IMG_STREAM *p_stream,void *p_dest,uint16 p_bytecount);
BYTE read_mem_stream_byte(CONVERT_IMG_STREAM *p_stream);
int16 read_param(CONVERT_IMG_STREAM *p_input,void *p_dest,uint16 p_bytecount);
BYTE read_param_byte(CONVERT_IMG_STREAM *p_input);
/****************************/    
/*END STREAM DECLARATIONS*/
/****************************/    


#ifdef __cplusplus
}
#endif

#endif


