/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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

// {C09EDB23-B7AF-11d2-B35E-525400E2D63A}
#define NS_IMIME_OBJECT_CLASS_ACCESS_IID \
      { 0xc09edb23, 0xb7af, 0x11d2,	 \
      { 0xb3, 0x5e, 0x52, 0x54, 0x0, 0xe2, 0xd6, 0x3a } }

class nsIMimeObjectClassAccess : public nsISupports {
public: 
  static const nsIID& GetIID() { static nsIID iid = NS_IMIME_OBJECT_CLASS_ACCESS_IID; return iid; }

  // These methods are all implemented by libmime to be used by 
  // content type handler plugins for processing stream data. 

  // This is the write call for outputting processed stream data.
  NS_IMETHOD    MimeObjectWrite(void *mimeObject, 
                                char *data, 
                                PRInt32 length, 
                                PRBool user_visible_p) = 0;

  // The following group of calls expose the pointers for the object
  // system within libmime.
  NS_IMETHOD    GetmimeInlineTextClass(void **ptr) = 0;
  NS_IMETHOD    GetmimeLeafClass(void **ptr) = 0;
  NS_IMETHOD    GetmimeObjectClass(void **ptr) = 0;
  NS_IMETHOD    GetmimeContainerClass(void **ptr) = 0;
  NS_IMETHOD    GetmimeMultipartClass(void **ptr) = 0;
  NS_IMETHOD    GetmimeMultipartSignedClass(void **ptr) = 0;
}; 

#endif /* nsIMimeObjectClassAccess_h_ */
