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

extern "C" int CAL_OUT_OF_MEMORY = -1000;

/*
 * These functions are the public interface for this content type
 * handler and will be called in by the mime component.
 */
#define      CAL_CONTENT_TYPE  "text/calendar"

 /* This is the object definition. Note: we will set the superclass 
    to NULL and manually set this on the class creation */
MimeDefClass(MimeInlineTextCalendar, MimeInlineTextCalendarClass,
             mimeInlineTextCalendarClass, NULL);  

extern "C" char *
MIME_GetContentType(void)
{
  return CAL_CONTENT_TYPE;
}

extern "C" MimeObjectClass *
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
  if (clazz->buffer == NULL) return CAL_OUT_OF_MEMORY;
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
    if (clazz->buffer == NULL) return CAL_OUT_OF_MEMORY;
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
