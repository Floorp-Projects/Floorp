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

/* 
 * This is the definitions for the Content Type Handler plugins for
 * libmime. This will allow developers the dynamically add the ability
 * for libmime to render new content types in the MHTML rendering of
 * HTML messages.
 */
 
#ifndef _MIMECTH_H_
#define _MIMECTH_H_

#include "mimei.h"
#include "modmime.h"
#include "mimeobj.h"	/*  MimeObject (abstract)							*/
#include "mimecont.h"	/*   |--- MimeContainer (abstract)					*/
#include "mimemult.h"	/*   |     |--- MimeMultipart (abstract)			*/
#include "mimemsig.h"	/*   |     |     |--- MimeMultipartSigned (abstract)*/
#include "mimetext.h"	/*   |     |--- MimeInlineText (abstract)			*/

/*
  This header exposes functions that are necessary to access the 
  object heirarchy for the mime chart. The class hierarchy is:

     MimeObject (abstract)
      |
      |--- MimeContainer (abstract)
      |     |
      |     |--- MimeMultipart (abstract)
      |     |     |
      |     |     |--- MimeMultipartMixed
      |     |     |
      |     |     |--- MimeMultipartDigest
      |     |     |
      |     |     |--- MimeMultipartParallel
      |     |     |
      |     |     |--- MimeMultipartAlternative
      |     |     |
      |     |     |--- MimeMultipartRelated
      |     |     |
      |     |     |--- MimeMultipartAppleDouble
      |     |     |
      |     |     |--- MimeSunAttachment
      |     |     |
      |     |     |--- MimeMultipartSigned (abstract)
      |     |          |
      |     |          |--- MimeMultipartSigned
      |     |
      |     |--- MimeXlateed (abstract)
      |     |     |
      |     |     |--- MimeXlateed
      |     |
      |     |--- MimeMessage
      |     |
      |     |--- MimeUntypedText
      |
      |--- MimeLeaf (abstract)
      |     |
      |     |--- MimeInlineText (abstract)
      |     |     |
      |     |     |--- MimeInlineTextPlain
      |     |     |
      |     |     |--- MimeInlineTextHTML
      |     |     |
      |     |     |--- MimeInlineTextRichtext
      |     |     |     |
      |     |     |     |--- MimeInlineTextEnriched
      |     |	    |
      |     |	    |--- MimeInlineTextVCard
      |     |	    |
      |     |	    |--- MimeInlineTextCalendar
      |     |
      |     |--- MimeInlineImage
      |     |
      |     |--- MimeExternalObject
      |
      |--- MimeExternalBody
 */

/*
 * These are used by libmime to locate the plugins. It uses
 * a directory under the bin directory for libmime to contain
 * the plugins. This directory is defined by MIME_PLUGIN_DIR and
 * mime content handler plugins have a name prefixed by the 
 * MIME_PLUGIN_PREFIX.
 */
#ifdef XP_WIN
#define   MIME_PLUGIN_PREFIX      "mimect-"
#else
#define   MIME_PLUGIN_PREFIX      "libmimect-"
#endif 

#define   MIME_PLUGIN_DIR         "mimeplugins"

typedef struct {
  PRBool      force_inline_display;
} contentTypeHandlerInitStruct;

/*
 * These functions are exposed by libmime to be used by content type
 * handler plugins for processing stream data. 
 */
/*
 * This is the write call for outputting processed stream data.
 */ 
extern int                        MIME_MimeObject_write(MimeObject *, char *data, 
                                                        PRInt32 length, 
                                                        PRBool user_visible_p);
/*
 * The following group of calls expose the pointers for the object
 * system within libmime. 
 */                                                        
extern MimeInlineTextClass       *MIME_GetmimeInlineTextClass(void);
extern MimeLeafClass             *MIME_GetmimeLeafClass(void);
extern MimeObjectClass           *MIME_GetmimeObjectClass(void);
extern MimeContainerClass        *MIME_GetmimeContainerClass(void);
extern MimeMultipartClass        *MIME_GetmimeMultipartClass(void);
extern MimeMultipartSignedClass  *MIME_GetmimeMultipartSignedClass(void);

/*
 * These are the functions that need to be implemented by the
 * content type handler plugin. They will be called by by libmime
 * when the module is loaded at runtime. 
 */
 
/* 
 * MIME_GetContentType() is called by libmime to identify the content
 * type handled by this plugin.
 */ 
char            *MIME_GetContentType(void);

/* 
 * This will create the MimeObjectClass object to be used by the libmime
 * object system.
 */
MimeObjectClass *MIME_CreateContentTypeHandlerClass(const char *content_type, 
                                   contentTypeHandlerInitStruct *initStruct);

/* 
 * Typedefs for libmime to use when locating and calling the above
 * defined functions.
 */
typedef char * (*mime_get_ct_fn_type)(void);
typedef MimeObjectClass * (*mime_create_class_fn_type)
                              (const char *, contentTypeHandlerInitStruct *);

#endif /* _MIMECTH_H_ */
