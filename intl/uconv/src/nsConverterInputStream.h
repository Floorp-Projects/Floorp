/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIInputStream.h"
#include "nsIConverterInputStream.h"
#include "nsIUnicharLineInputStream.h"
#include "nsString.h"
#include "nsReadLine.h"

#include "nsCOMPtr.h"
#include "nsIUnicodeDecoder.h"
#include "nsIByteBuffer.h"
#include "nsIUnicharBuffer.h"

#define NS_CONVERTERINPUTSTREAM_CONTRACTID "@mozilla.org/intl/converter-input-stream;1"

// {2BC2AD62-AD5D-4b7b-A9DB-F74AE203C527}
#define NS_CONVERTERINPUTSTREAM_CID \
  { 0x2bc2ad62, 0xad5d, 0x4b7b, \
   { 0xa9, 0xdb, 0xf7, 0x4a, 0xe2, 0x3, 0xc5, 0x27 } }



class nsConverterInputStream : public nsIConverterInputStream,
                               public nsIUnicharLineInputStream {

 public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIUNICHARINPUTSTREAM
    NS_DECL_NSIUNICHARLINEINPUTSTREAM
    NS_DECL_NSICONVERTERINPUTSTREAM

    nsConverterInputStream() :
        mLastErrorCode(NS_OK),
        mLeftOverBytes(0),
        mUnicharDataOffset(0),
        mUnicharDataLength(0),
        mReplacementChar(DEFAULT_REPLACEMENT_CHARACTER),
        mLineBuffer(nsnull) { }
    
    virtual ~nsConverterInputStream() { Close(); }

 private:


    PRUint32 Fill(nsresult *aErrorCode);
    
    nsCOMPtr<nsIUnicodeDecoder> mConverter;
    nsCOMPtr<nsIByteBuffer> mByteData;
    nsCOMPtr<nsIUnicharBuffer> mUnicharData;
    nsCOMPtr<nsIInputStream> mInput;

    nsresult  mLastErrorCode;
    PRUint32  mLeftOverBytes;
    PRUint32  mUnicharDataOffset;
    PRUint32  mUnicharDataLength;
    PRUnichar mReplacementChar;

    nsLineBuffer<PRUnichar>* mLineBuffer;    
};
