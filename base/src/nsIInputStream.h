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
#ifndef nsIInputStream_h___
#define nsIInputStream_h___

#include "nsIBaseStream.h"

#define NS_IINPUTSTREAM_IID   \
{ 0x022396f0, 0x93b5, 0x11d1, \
  {0x89, 0x5b, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81} }

/** Abstract byte input stream */
class nsIInputStream : public nsIBaseStream {
public:
  /** Read data from the stream.
   *  @param aErrorCode the error code if an error occurs
   *  @param aBuf the buffer into which the data is read
   *  @param aOffset the start offset of the data
   *  @param aCount the maximum number of bytes to read
   *  @return number of bytes read or -1 if error
   */   
  virtual PRInt32 Read(PRInt32* aErrorCode,
                       char* aBuf,
                       PRInt32 aOffset,
                       PRInt32 aCount) = 0;
};

/** Error codes */
//@{ 
// XXX fix up the values to work with nsqresult
/// End of file
#define NS_INPUTSTREAM_EOF 1
/// Stream closed
#define NS_INPUTSTREAM_CLOSED 2
/// Error from the operating system
#define NS_INPUTSTREAM_OSERROR 3
/// Illegal arguments
#define NS_INPUTSTREAM_ILLEGAL_ARGS 4
/// For unichar streams
#define NS_INPUTSTREAM_NO_CONVERTER 5
/// For unichar streams
#define NS_INPUTSTREAM_BAD_CONVERSION 6
//@}

#endif /* nsInputStream_h___ */
