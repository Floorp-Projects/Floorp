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

/* 
 * This is the definitions for the Content Type Handler plugins for
 * libmime. This will allow developers the dynamically add the ability
 * for libmime to render new content types in the MHTML rendering of
 * HTML messages.
 */
 
#ifndef _MIMECTH_H_
#define _MIMECTH_H_

#include "mimei.h"
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

#include "nsIMimeContentTypeHandler.h"

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
extern "C" 
char            *MIME_GetContentType(void);

/* 
 * This will create the MimeObjectClass object to be used by the libmime
 * object system.
 */
extern "C" 
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
