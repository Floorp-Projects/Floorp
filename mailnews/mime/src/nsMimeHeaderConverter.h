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
#ifndef nsMimeHeaderConverter_h_
#define nsMimeHeaderConverter_h_

#include "prtypes.h"
#include "nsIMimeHeaderConverter.h"

// {866A1E11-D0B9-11d2-B373-525400E2D63A}
#define NS_MIME_HEADER_CONVERTER_CID   \
        { 0x866a1e11, 0xd0b9, 0x11d2,         \
        { 0xb3, 0x73, 0x52, 0x54, 0x0, 0xe2, 0xd6, 0x3a } };

class nsMimeHeaderConverter : public nsIMimeHeaderConverter {
public: 
  nsMimeHeaderConverter();
  virtual ~nsMimeHeaderConverter();
       
  /* this macro defines QueryInterface, AddRef and Release for this class */
  NS_DECL_ISUPPORTS 

  // These methods are all implemented by libmime to be used by 
  // content type handler plugins for processing stream data. 

  // Decode routine
  NS_IMETHOD DecodeMimePartIIStr(const char *header, 
                                 char       *charset, 
                                 char **decodedString);

  // Decode routine (also converts output to unicode)
  NS_IMETHOD DecodeMimePartIIStr(const nsString& header, 
                                 nsString& charset, 
                                 nsString& decodedString);
  // Encode routine
  NS_IMETHOD EncodeMimePartIIStr(const char    *header, 
                                 const char    *mailCharset, 
                                 const PRInt32 encodedWordSize, 
                                 char          **encodedString);
}; 

/* this function will be used by the factory to generate an class access object....*/
extern nsresult NS_NewMimeHeaderConverter(nsIMimeHeaderConverter **aInstancePtrResult);

#endif /* nsMimeHeaderConverter_h_ */
