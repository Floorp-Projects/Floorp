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
#include "mimecal.h"
#include "xp_core.h"
#include "xp_mem.h"
#include "prmem.h"
#include "plstr.h"
#include "mimexpcom.h"
#include "mimecth.h"
#include "mimeobj.h"
#include "nsCRT.h"

static int MimeInlineTextCalendar_parse_line (char *, PRInt32, MimeObject *);
static int MimeInlineTextCalendar_parse_eof (MimeObject *, PRBool);
static int MimeInlineTextCalendar_parse_begin (MimeObject *obj);

extern int MK_OUT_OF_MEMORY;


/*
 * These functions are the public interface for this content type
 * handler and will be called in by the mime component.
 */
#define      CAL_CONTENT_TYPE  "text/calendar"

 /* This is the object definition. Note: we will set the superclass 
    to NULL and manually set this on the class creation */
MimeDefClass(MimeInlineTextCalendar, MimeInlineTextCalendarClass,
             mimeInlineTextCalendarClass, NULL);  

PUBLIC char *
MIME_GetContentType(void)
{
  return CAL_CONTENT_TYPE;
}

PUBLIC MimeObjectClass *
MIME_CreateContentTypeHandlerClass(const char *content_type, 
                                   contentTypeHandlerInitStruct *initStruct)
{
  MimeObjectClass *clazz = (MimeObjectClass *)&mimeInlineTextCalendarClass;
  /*
   * Must set the superclass by hand.
   */
  if (!COM_GetmimeInlineTextClass())
    return NULL;

  clazz->superclass = (MimeObjectClass *)COM_GetmimeInlineTextClass();
  initStruct->force_inline_display = PR_FALSE;
  return clazz;
}

static int
MimeInlineTextCalendarClassInitialize(MimeInlineTextCalendarClass *clazz)
{
  MimeObjectClass *oclass = (MimeObjectClass *) clazz;
  PR_ASSERT(!oclass->class_initialized);
  oclass->parse_begin = MimeInlineTextCalendar_parse_begin;
  oclass->parse_line  = MimeInlineTextCalendar_parse_line;
  oclass->parse_eof   = MimeInlineTextCalendar_parse_eof;

  return 0;
}

int
mime_TranslateCalendar(char* caldata, char** html) 
{
  *html = nsCRT::strdup("\
<text=\"#000000\" bgcolor=\"#FFFFFF\" link=\"#FF0000\" vlink=\"#800080\" alink=\"#0000FF\">\
<center><table BORDER=1 BGCOLOR=\"#CCFFFF\" ><tr>\
<td>This libmime Content Type Handler plugin for the Content-Type <b>text/calendar\
</b> is functional!</td>\
</tr>\
</table></center>");

  return 0;
}

static int
MimeInlineTextCalendar_parse_begin(MimeObject *obj)
{
  MimeInlineTextCalendarClass *clazz;
  int status = ((MimeObjectClass*)COM_GetmimeLeafClass())->parse_begin(obj);
  
  if (status < 0) return status;
  
  if (!obj->output_p) return 0;
  if (!obj->options || !obj->options->write_html_p) return 0;
  
  /* This is a fine place to write out any HTML before the real meat begins. */  
  clazz = ((MimeInlineTextCalendarClass *) obj->clazz);
  /* initialize buffered string to empty; */
  
  clazz->bufferlen = 0;
  clazz->buffermax = 512;
  clazz->buffer = (char*) PR_MALLOC(clazz->buffermax);
  if (clazz->buffer == NULL) return MK_OUT_OF_MEMORY;
  return 0;
}

static int
MimeInlineTextCalendar_parse_line(char *line, PRInt32 length, MimeObject *obj)
{
 /* 
  * This routine gets fed each line of data, one at a time. We just buffer
  * it all up, to be dealt with all at once at the end. 
  */  
  MimeInlineTextCalendarClass *clazz = ((MimeInlineTextCalendarClass *) obj->clazz);
  
  if (!obj->output_p) return 0;
  if (!obj->options || !obj->options->output_fn) return 0;
  if (!obj->options->write_html_p) {
    return COM_MimeObject_write(obj, line, length, TRUE);
  }
  
  if (clazz->bufferlen + length >= clazz->buffermax) {
    do {
      clazz->buffermax += 512;
    } while (clazz->bufferlen + length >= clazz->buffermax);
    clazz->buffer = (char *)PR_Realloc(clazz->buffer, clazz->buffermax);
    if (clazz->buffer == NULL) return MK_OUT_OF_MEMORY;
  }
  nsCRT::memcpy(clazz->buffer + clazz->bufferlen, line, length);
  clazz->bufferlen += length;
  return 0;
}

static int
MimeInlineTextCalendar_parse_eof (MimeObject *obj, PRBool abort_p)
{
  int status = 0;
  MimeInlineTextCalendarClass *clazz = ((MimeInlineTextCalendarClass *) obj->clazz);
  char* html = NULL;
  
  if (obj->closed_p) return 0;
  
  /* Run parent method first, to flush out any buffered data. */
  status = ((MimeObjectClass*)COM_GetmimeInlineTextClass())->parse_eof(obj, abort_p);
  if (status < 0) return status;
  
  if (!clazz->buffer || clazz->bufferlen == 0) return 0;
  
  clazz->buffer[clazz->bufferlen] = '\0';
  
  status = mime_TranslateCalendar(clazz->buffer, &html);
  PR_Free(clazz->buffer);
  clazz->buffer = NULL;
  if (status < 0) return status;
  
  status = COM_MimeObject_write(obj, html, PL_strlen(html), TRUE);
  PR_Free(html);
  if (status < 0) return status;
  
  return 0;
}
