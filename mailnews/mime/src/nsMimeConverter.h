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
#ifndef nsMimeConverter_h_
#define nsMimeConverter_h_

#include "prtypes.h"
#include "nsIMimeConverter.h"

class nsMimeConverter : public nsIMimeConverter {
public: 
  nsMimeConverter();
  virtual ~nsMimeConverter();
       
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

  // Encode routine (utf-8 input)
  NS_IMETHOD EncodeMimePartIIStr_UTF8(const char    *header, 
                                      const char    *mailCharset, 
                                      const PRInt32 encodedWordSize, 
                                      char          **encodedString);

  NS_IMETHOD B64EncoderInit(int (*output_fn) (const char *buf, PRInt32 size, void *closure), 
                                void *closure, MimeEncoderData **returnEncoderData);

  NS_IMETHOD QPEncoderInit (int (*output_fn) (const char *buf, 
                                PRInt32 size, void *closure), void *closure, 
                                MimeEncoderData ** returnEncoderData);

  NS_IMETHOD UUEncoderInit (char *filename, int (*output_fn) 
                               (const char *buf, PRInt32 size, void *closure), void *closure, 
                               MimeEncoderData ** returnEncoderData);

  NS_IMETHOD EncoderDestroy(MimeEncoderData *data, PRBool abort_p);

  NS_IMETHOD EncoderWrite (MimeEncoderData *data, const char *buffer, PRInt32 size, PRInt32 *written);

}; 

/* this function will be used by the factory to generate an class access object....*/
extern nsresult NS_NewMimeConverter(nsIMimeConverter **aInstancePtrResult);

#endif /* nsMimeConverter_h_ */
