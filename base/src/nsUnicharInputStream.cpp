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
#include "nsIUnicharInputStream.h"
#include "nsIByteBuffer.h"
#include "nsIUnicharBuffer.h"
#include "nsString.h"
#include "nsCRT.h"
#include <fcntl.h>
#ifdef NS_WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

static NS_DEFINE_IID(kIUnicharInputStreamIID, NS_IUNICHAR_INPUT_STREAM_IID);

class StringUnicharInputStream : public nsIUnicharInputStream {
public:
  StringUnicharInputStream(nsString* aString);
  ~StringUnicharInputStream();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Read(PRUnichar* aBuf,
                  PRUint32 aOffset,
                  PRUint32 aCount,
                  PRUint32 *aReadCount);
  NS_IMETHOD Close();

  nsString* mString;
  PRUint32 mPos;
  PRUint32 mLen;
};

StringUnicharInputStream::StringUnicharInputStream(nsString* aString)
{
  NS_INIT_REFCNT();
  mString = aString;
  mPos = 0;
  mLen = aString->Length();
}

StringUnicharInputStream::~StringUnicharInputStream()
{
  if (nsnull != mString) {
    delete mString;
  }
}

nsresult StringUnicharInputStream::Read(PRUnichar* aBuf,
                                        PRUint32 aOffset,
                                        PRUint32 aCount,
                                        PRUint32 *aReadCount)
{
  if (mPos >= mLen) {
    *aReadCount = 0;
    return (nsresult)-1;
  }
  const PRUnichar* us = mString->GetUnicode();
  NS_ASSERTION(mLen >= mPos, "unsigned madness");
  PRUint32 amount = mLen - mPos;
  if (amount > aCount) {
    amount = aCount;
  }
  nsCRT::memcpy(aBuf + aOffset, us + mPos, sizeof(PRUnichar) * amount);
  mPos += amount;
  *aReadCount = amount;
  return NS_OK;
}

nsresult StringUnicharInputStream::Close()
{
  mPos = mLen;
  if (nsnull != mString) {
    delete mString;
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS(StringUnicharInputStream, kIUnicharInputStreamIID);

NS_BASE nsresult
NS_NewStringUnicharInputStream(nsIUnicharInputStream** aInstancePtrResult,
                               nsString* aString)
{
  NS_PRECONDITION(nsnull != aString, "null ptr");
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if ((nsnull == aString) || (nsnull == aInstancePtrResult)) {
    return NS_ERROR_NULL_POINTER;
  }

  StringUnicharInputStream* it = new StringUnicharInputStream(aString);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kIUnicharInputStreamIID,
                            (void**) aInstancePtrResult);
}

//----------------------------------------------------------------------

class IsoLatin1Converter : public nsIB2UConverter {
public:
  IsoLatin1Converter();

  NS_DECL_ISUPPORTS
  NS_IMETHOD Convert(PRUnichar* aDst,
                     PRUint32 aDstOffset,
                     PRUint32& aDstLen,
                     const char* aSrc,
                     PRUint32 aSrcOffset,
                     PRUint32& aSrcLen);
};

IsoLatin1Converter::IsoLatin1Converter()
{
  NS_INIT_REFCNT();
}

NS_DEFINE_IID(kIB2UConverterIID, NS_IB2UCONVERTER_IID);
NS_IMPL_ISUPPORTS(IsoLatin1Converter,kIB2UConverterIID);

nsresult IsoLatin1Converter::Convert(PRUnichar* aDst,
                                    PRUint32 aDstOffset,
                                    PRUint32& aDstLen,
                                    const char* aSrc,
                                    PRUint32 aSrcOffset,
                                    PRUint32& aSrcLen)
{
  PRUint32 amount = aSrcLen;
  if (aSrcLen > aDstLen) {
    amount = aDstLen;
  }
  const char* end = aSrc + amount;
  while (aSrc < end) {
    PRUint8 isoLatin1 = PRUint8(*aSrc++);
    /* XXX insert table based lookup converter here */
    *aDst++ = isoLatin1;
  }
  aDstLen = amount;
  aSrcLen = amount;
  return NS_OK;
}

NS_BASE nsresult
NS_NewB2UConverter(nsIB2UConverter** aInstancePtrResult,
                   nsISupports* aOuter,
                   nsString* aCharSet)
{
  if (nsnull != aOuter) {
    return NS_ERROR_NO_AGGREGATION;
  }
  // We cannot use enum to pass charset id
  if ((nsnull != aCharSet) && (! aCharSet->EqualsIgnoreCase( "iso-8859-1" ))){
      return NS_BASE_STREAM_NO_CONVERTER;
  }
  IsoLatin1Converter* it = new IsoLatin1Converter();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIB2UConverterIID, (void**)aInstancePtrResult);
}

//----------------------------------------------------------------------

class ConverterInputStream : public nsIUnicharInputStream {
public:
  ConverterInputStream(nsIInputStream* aStream,
                       nsIB2UConverter* aConverter,
                       PRUint32 aBufSize);
  ~ConverterInputStream();

  NS_DECL_ISUPPORTS
  NS_IMETHOD Read(PRUnichar* aBuf,
                  PRUint32 aOffset,
                  PRUint32 aCount,
                  PRUint32 *aReadCount);
  NS_IMETHOD Close();

protected:
  PRInt32 Fill(nsresult * aErrorCode);

  nsIInputStream* mInput;
  nsIB2UConverter* mConverter;
  nsIByteBuffer* mByteData;
  PRUint32 mByteDataOffset;
  nsIUnicharBuffer* mUnicharData;
  PRUint32 mUnicharDataOffset;
  PRUint32 mUnicharDataLength;
};

ConverterInputStream::ConverterInputStream(nsIInputStream* aStream,
                                           nsIB2UConverter* aConverter,
                                           PRUint32 aBufferSize)
{
  NS_INIT_REFCNT();
  mInput = aStream; aStream->AddRef();
  mConverter = aConverter; aConverter->AddRef();
  if (aBufferSize == 0) {
    aBufferSize = 8192;
  }
  nsresult rv1 = NS_NewByteBuffer(&mByteData, nsnull, aBufferSize);
  nsresult rv2 = NS_NewUnicharBuffer(&mUnicharData, nsnull, aBufferSize);
  mByteDataOffset = 0;
  mUnicharDataOffset = 0;
  mUnicharDataLength = 0;
}

NS_IMPL_ISUPPORTS(ConverterInputStream,kIUnicharInputStreamIID);

ConverterInputStream::~ConverterInputStream()
{
  Close();
}

nsresult ConverterInputStream::Close()
{
  if (nsnull != mInput) {
    mInput->Release();
    mInput = nsnull;
  }
  if (nsnull != mConverter) {
    mConverter->Release();
    mConverter = nsnull;
  }
  if (nsnull != mByteData) {
    mByteData->Release();
    mByteData = nsnull;
  }
  if (nsnull != mUnicharData) {
    mUnicharData->Release();
    mUnicharData = nsnull;
  }

  return NS_OK;
}

nsresult ConverterInputStream::Read(PRUnichar* aBuf,
                                    PRUint32 aOffset,
                                    PRUint32 aCount,
                                    PRUint32 *aReadCount)
{
  NS_ASSERTION(mUnicharDataLength >= mUnicharDataOffset, "unsigned madness");
  PRUint32 rv = mUnicharDataLength - mUnicharDataOffset;
  nsresult errorCode;
  if (0 == rv) {
    // Fill the unichar buffer
    rv = Fill(&errorCode);
    if (rv <= 0) {
      *aReadCount = 0;
      return errorCode;
    }
  }
  if (rv > aCount) {
    rv = aCount;
  }
  nsCRT::memcpy(aBuf + aOffset, mUnicharData->GetBuffer() + mUnicharDataOffset,
                rv * sizeof(PRUnichar));
  mUnicharDataOffset += rv;
  *aReadCount = rv;
  return NS_OK;
}

PRInt32 ConverterInputStream::Fill(nsresult * aErrorCode)
{
  if (nsnull == mInput) {
    // We already closed the stream!
    *aErrorCode = NS_BASE_STREAM_CLOSED;
    return -1;
  }

  NS_ASSERTION(mByteData->GetLength() >= mByteDataOffset, "unsigned madness");
  PRUint32 remainder = mByteData->GetLength() - mByteDataOffset;
  mByteDataOffset = remainder;
  PRInt32 nb = mByteData->Fill(aErrorCode, mInput, remainder);
  if (nb <= 0) {
    // Because we assume a many to one conversion, the lingering data
    // in the byte buffer must be a partial conversion
    // fragment. Because we know that we have recieved no more new
    // data to add to it, we can't convert it. Therefore, we discard
    // it.
    return nb;
  }
  NS_ASSERTION(remainder + nb == mByteData->GetLength(), "bad nb");

  // Now convert as much of the byte buffer to unicode as possible
  PRUint32 dstLen = mUnicharData->GetBufferSize();
  PRUint32 srcLen = remainder + nb;
  *aErrorCode = mConverter->Convert(mUnicharData->GetBuffer(), 0, dstLen,
                                    mByteData->GetBuffer(), 0, srcLen);
  mUnicharDataOffset = 0;
  mUnicharDataLength = dstLen;
  mByteDataOffset += srcLen;
  return dstLen;
}

// XXX hook up auto-detect here (do we need more info, like the url?)
NS_BASE nsresult
NS_NewConverterStream(nsIUnicharInputStream** aInstancePtrResult,
                      nsISupports* aOuter,
                      nsIInputStream* aStreamToWrap,
                      PRInt32 aBufferSize,
                      nsString* aCharSet)
{
  if (nsnull != aOuter) {
    return NS_ERROR_NO_AGGREGATION;
  }

  // Create converter
  nsIB2UConverter* converter;
  nsresult rv = NS_NewB2UConverter(&converter, nsnull, aCharSet);
  if (NS_OK != rv) {
    return rv;
  }

  // Create converter input stream
  ConverterInputStream* it =
    new ConverterInputStream(aStreamToWrap, converter, aBufferSize);
  converter->Release();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIUnicharInputStreamIID, 
                            (void **) aInstancePtrResult);
}
