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
 * This interface is implemented by libmime. This interface is used by 
 * a Content-Type handler "Plug In" (i.e. vCard) for accessing various 
 * internal information about the object class system of libmime. When 
 * libmime progresses to a C++ object class, this would probably change.
 */
#ifndef nsIMimeObjectClassAccess_h_
#define nsIMimeObjectClassAccess_h_

#include "prtypes.h"

//
// This is to access the object class of libmime which is what
// this interface is focused on. 
//
#include "mimei.h"
#include "modmime.h"
#include "mimeobj.h"	/*  MimeObject (abstract)							*/
#include "mimecont.h"	/*   |--- MimeContainer (abstract)					*/
#include "mimemult.h"	/*   |     |--- MimeMultipart (abstract)			*/
#include "mimemsig.h"	/*   |     |     |--- MimeMultipartSigned (abstract)*/
#include "mimetext.h"	/*   |     |--- MimeInlineText (abstract)			*/

// {C09EDB23-B7AF-11d2-B35E-525400E2D63A}
#define NS_ICONTENT_TYPE_PLUGIN_IID \
      { 0xc09edb23, 0xb7af, 0x11d2, 
      { 0xb3, 0x5e, 0x52, 0x54, 0x0, 0xe2, 0xd6, 0x3a } };

typedef struct {
  PRBool      force_inline_display;
} contentTypeHandlerInitStruct;

class nsIMimeObjectClassAccess { 

public: 
  // These methods are all implemented by libmime to be used by 
  // content type handler plugins for processing stream data. 

  // This is the write call for outputting processed stream data.
  NS_IMETHOD    MimeObjectWrite(MimeObject *obj, 
                                char *data, 
                                PRInt32 length, 
                                PRBool user_visible_p) = 0;

  // The following group of calls expose the pointers for the object
  // system within libmime.
  NS_IMETHOD    GetmimeInlineTextClass(MimeInlineTextClass **ptr) = 0;
  NS_IMETHOD    GetmimeLeafClass(MimeLeafClass **ptr) = 0;
  NS_IMETHOD    GetmimeObjectClass(MimeObjectClass **ptr) = 0;
  NS_IMETHOD    GetmimeContainerClass(MimeContainerClass **ptr) = 0;
  NS_IMETHOD    GetmimeMultipartClass(MimeMultipartClass **ptr) = 0;
  NS_IMETHOD    GetmimeMultipartSignedClass(MimeMultipartSignedClass **ptr) = 0;
}; 

#endif /* nsIMimeObjectClassAccess_h_ */
