/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#ifndef nsIOutputStream_h___
#define nsIOutputStream_h___

#include "nsIBaseStream.h"

/* 7f13b870-e95f-11d1-beae-00805f8a66dc */
#define NS_IOUTPUTSTREAM_IID   \
{ 0x7f13b870, 0xe95f, 0x11d1, \
  {0xbe, 0xae, 0x00, 0x80, 0x5f, 0x8a, 0x66, 0xdc} }

/** Abstract byte input stream */
class nsIOutputStream : public nsIBaseStream {
public:
  /** Write data into the stream.
   *  @param aErrorCode the error code if an error occurs
   *  @param aBuf the buffer into which the data is read
   *  @param aOffset the start offset of the data
   *  @param aCount the maximum number of bytes to read
   *  @return number of bytes read or -1 if error
   */   
  virtual PRInt32 Write(PRInt32* aErrorCode,
                        const char* aBuf,
                        PRInt32 aOffset,
                        PRInt32 aCount) = 0;
};


#endif /* nsOutputStream_h___ */
