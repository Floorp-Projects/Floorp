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
#ifndef nsIUnicharInputStream_h___
#define nsIUnicharInputStream_h___

#include "nsIInputStream.h"
class nsString;

#define NS_IUNICHAR_INPUT_STREAM_IID \
{ 0x2d97fbf0, 0x93b5, 0x11d1,        \
  {0x89, 0x5b, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81} }

#define NS_IB2UCONVERTER_IID  \
{ 0x35e40290, 0x93b5, 0x11d1, \
  {0x89, 0x5b, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81} }

/// Enumeration of character set ids.
enum nsCharSetID {
  eCharSetID_IsoLatin1 = 0,
  eCharSetID_UTF8,
  eCharSetID_ShiftJis
  // XXX more i'm sure...
};

/** Abstract unicode character input stream
 *  @see nsIInputStream
 */
class nsIUnicharInputStream : public nsISupports {
public:
  virtual PRInt32 Read(PRInt32* aErrorCode,
                       PRUnichar* aBuf,
                       PRInt32 aOffset,
                       PRInt32 aCount) = 0;
  virtual void Close() = 0;
};

/**
 * Create a nsIUnicharInputStream that wraps up a string. Data is fed
 * from the string out until the done. When this object is destroyed
 * it destroyes the string (so make a copy if you don't want it doing
 * that)
 */
extern NS_BASE nsresult
  NS_NewStringUnicharInputStream(nsIUnicharInputStream** aInstancePtrResult,
                                 nsString* aString);

/// Abstract interface for converting from bytes to unicode characters
class nsIB2UConverter : public nsISupports {
public:
  /** aDstLen is updated to indicate how much data was translated into
   * aDst; aSrcLen is updated to indicate how much data was used in
   * the source buffer.
   */
  virtual PRInt32 Convert(PRUnichar* aDst,
                          PRInt32 aDstOffset,
                          PRInt32& aDstLen,
                          const char* aSrc,
                          PRInt32 aSrcOffset,
                          PRInt32& aSrcLen) = 0;
};

/** Create a new nsUnicharInputStream that provides a converter for the
 * byte input stream aStreamToWrap. If no converter can be found then
 * nsnull is returned and the error code is set to
 * NS_INPUTSTREAM_NO_CONVERTER.
 */
extern NS_BASE nsresult
  NS_NewConverterStream(nsIUnicharInputStream** aInstancePtrResult,
                        nsISupports* aOuter,
                        nsIInputStream* aStreamToWrap,
                        PRInt32 aBufferSize = 0,
                        nsCharSetID aCharSet = eCharSetID_IsoLatin1);

/** Create a new nsB2UConverter for the given character set. When given
 * nsnull, the converter for iso-latin1 to unicode is provided. If no
 * converter can be found, nsnull is returned.
 */
extern NS_BASE nsresult
  NS_NewB2UConverter(nsIB2UConverter** aInstancePtrResult,
                     nsISupports* aOuter,
                     nsCharSetID aCharSet = eCharSetID_IsoLatin1);

#endif /* nsUnicharInputStream_h___ */
