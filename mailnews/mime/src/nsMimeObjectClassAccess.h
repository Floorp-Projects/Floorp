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
#ifndef nsMimeObjectClassAccess_h_
#define nsMimeObjectClassAccess_h_

#include "prtypes.h"
#include "nsIMimeObjectClassAccess.h"

// {403B0540-B7C3-11d2-B35E-525400E2D63A}
#define NS_MIME_OBJECT_CLASS_ACCESS_CID   \
        { 0x403b0540, 0xb7c3, 0x11d2,         \
        { 0xb3, 0x5e, 0x52, 0x54, 0x0, 0xe2, 0xd6, 0x3a } };

class nsMimeObjectClassAccess : public nsIMimeObjectClassAccess {
public: 
  nsMimeObjectClassAccess();
  virtual ~nsMimeObjectClassAccess();
       
  /* this macro defines QueryInterface, AddRef and Release for this class */
  NS_DECL_ISUPPORTS 

  // These methods are all implemented by libmime to be used by 
  // content type handler plugins for processing stream data. 

  // This is the write call for outputting processed stream data.
  NS_IMETHOD    MimeObjectWrite(void *mimeObject, 
                                char *data, 
                                PRInt32 length, 
                                PRBool user_visible_p);

  // The following group of calls expose the pointers for the object
  // system within libmime.
  NS_IMETHOD    GetmimeInlineTextClass(void **ptr);
  NS_IMETHOD    GetmimeLeafClass(void **ptr);
  NS_IMETHOD    GetmimeObjectClass(void **ptr);
  NS_IMETHOD    GetmimeContainerClass(void **ptr);
  NS_IMETHOD    GetmimeMultipartClass(void **ptr);
  NS_IMETHOD    GetmimeMultipartSignedClass(void **ptr);
}; 

/* this function will be used by the factory to generate an class access object....*/
extern nsresult NS_NewMimeObjectClassAccess(nsIMimeObjectClassAccess **aInstancePtrResult);

#endif /* nsMimeObjectClassAccess_h_ */
