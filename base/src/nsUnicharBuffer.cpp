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
#include "nsIUnicharBuffer.h"
#include "nsIUnicharInputStream.h"
#include "nsCRT.h"

#define MIN_BUFFER_SIZE 32

class UnicharBufferImpl : public nsIUnicharBuffer {
public:
  UnicharBufferImpl(PRUint32 aBufferSize);
  virtual ~UnicharBufferImpl();

  NS_DECL_ISUPPORTS
  virtual PRInt32 GetLength() const;
  virtual PRInt32 GetBufferSize() const;
  virtual PRUnichar* GetBuffer() const;
  virtual PRBool Grow(PRInt32 aNewSize);
  virtual PRInt32 Fill(nsresult* aErrorCode, nsIUnicharInputStream* aStream,
                       PRInt32 aKeep);

  PRUnichar* mBuffer;
  PRUint32 mSpace;
  PRUint32 mLength;
};

UnicharBufferImpl::UnicharBufferImpl(PRUint32 aBufferSize)
{
  if (aBufferSize < MIN_BUFFER_SIZE) {
    aBufferSize = MIN_BUFFER_SIZE;
  }
  mSpace = aBufferSize;
  mBuffer = new PRUnichar[aBufferSize];
  mLength = 0;
  NS_INIT_REFCNT();
}

NS_DEFINE_IID(kUnicharBufferIID, NS_IUNICHAR_BUFFER_IID);
NS_IMPL_ISUPPORTS(UnicharBufferImpl,kUnicharBufferIID)

UnicharBufferImpl::~UnicharBufferImpl()
{
  if (nsnull != mBuffer) {
    delete[] mBuffer;
    mBuffer = nsnull;
  }
  mLength = 0;
}

PRInt32 UnicharBufferImpl::GetLength() const
{
  return mLength;
}

PRInt32 UnicharBufferImpl::GetBufferSize() const
{
  return mSpace;
}

PRUnichar* UnicharBufferImpl::GetBuffer() const
{
  return mBuffer;
}

PRBool UnicharBufferImpl::Grow(PRInt32 aNewSize)
{
  if (PRUint32(aNewSize) < MIN_BUFFER_SIZE) {
    aNewSize = MIN_BUFFER_SIZE;
  }
  PRUnichar* newbuf = new PRUnichar[aNewSize];
  if (nsnull != newbuf) {
    if (0 != mLength) {
      nsCRT::memcpy(newbuf, mBuffer, mLength * sizeof(PRUnichar));
    }
    delete[] mBuffer;
    mBuffer = newbuf;
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRInt32 UnicharBufferImpl::Fill(nsresult* aErrorCode,
                                nsIUnicharInputStream* aStream,
                                PRInt32 aKeep)
{
  NS_PRECONDITION(nsnull != aStream, "null stream");
  NS_PRECONDITION(PRUint32(aKeep) < PRUint32(mLength), "illegal keep count");
  if ((nsnull == aStream) || (PRUint32(aKeep) >= PRUint32(mLength))) {
    // whoops
    *aErrorCode = NS_BASE_STREAM_ILLEGAL_ARGS;
    return -1;
  }

  if (0 != aKeep) {
    // Slide over kept data
    nsCRT::memmove(mBuffer, mBuffer + (mLength - aKeep),
                   aKeep * sizeof(PRUnichar));
  }

  // Read in some new data
  mLength = aKeep;
  PRInt32 amount = mSpace - aKeep;
  PRUint32 nb;
  NS_ASSERTION(aKeep >= 0, "unsigned madness");
  NS_ASSERTION(amount >= 0, "unsigned madness");
  *aErrorCode = aStream->Read(mBuffer, (PRUint32)aKeep, (PRUint32)amount, &nb);
  if (NS_SUCCEEDED(*aErrorCode)) {
    mLength += nb;
  }
  else
    nb = 0;
  return nb;
}

NS_BASE nsresult NS_NewUnicharBuffer(nsIUnicharBuffer** aInstancePtrResult,
                                     nsISupports* aOuter,
                                     PRUint32 aBufferSize)
{
  if (nsnull != aOuter) {
    return NS_ERROR_NO_AGGREGATION;
  }
  UnicharBufferImpl* it = new UnicharBufferImpl(aBufferSize);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kUnicharBufferIID, (void **) aInstancePtrResult);
}
