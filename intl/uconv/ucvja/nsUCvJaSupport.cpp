/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "pratom.h"
#include "nsIComponentManager.h"
#include "nsUCvJaSupport.h"
#include "nsUCVJADll.h"

//----------------------------------------------------------------------
// Class nsEncoderSupport [implementation]

nsEncoderSupport::nsEncoderSupport() 
{
  mBufferCapacity = 16;
  mBuffer = new char[mBufferCapacity];

  mErrBehavior = kOnError_Signal;
  mErrChar = 0;
  mErrEncoder = NULL;

  Reset();
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
}

nsEncoderSupport::~nsEncoderSupport() 
{
  delete [] mBuffer;
  NS_IF_RELEASE(mErrEncoder);
  PR_AtomicDecrement(&g_InstanceCount);
}

NS_IMETHODIMP nsEncoderSupport::ConvertNoBuff(const PRUnichar * aSrc, 
                                              PRInt32 * aSrcLength, 
                                              char * aDest, 
                                              PRInt32 * aDestLength)
{
  // we do all operations using pointers internally
  const PRUnichar * src = aSrc;
  const PRUnichar * srcEnd = aSrc + *aSrcLength;
  char * dest = aDest;
  char * destEnd = aDest + *aDestLength;

  PRInt32 bcr, bcw; // byte counts for read & write;
  nsresult res;

  for (;;) {
    bcr = srcEnd - src;
    bcw = destEnd - dest;
    res = ConvertNoBuffNoErr(src, &bcr, dest, &bcw);
    src += bcr;
    dest += bcw;

    if (res == NS_ERROR_UENC_NOMAPPING) {
      if (mErrBehavior == kOnError_Replace) {
        const PRUnichar buff[] = {mErrChar};
        bcr = 1;
        bcw = destEnd - dest;
        src--; // back the input: maybe the guy won't consume consume anything.
        res = ConvertNoBuffNoErr(buff, &bcr, dest, &bcw);
        src += bcr;
        dest += bcw;
        if (res != NS_OK) break;
      } else if (mErrBehavior == kOnError_CallBack) {
        bcw = destEnd - dest;
        src--;
        res = mErrEncoder->Convert(*src, dest, &bcw);
        dest += bcw;
        // if enought output space then the last char was used
        if (res != NS_OK_UENC_MOREOUTPUT) src++;
        if (res != NS_OK) break;
      } else break;
    }
    else break;
  }

  *aSrcLength   -= srcEnd - src;
  *aDestLength  -= destEnd - dest;
  return res;
}

NS_IMETHODIMP nsEncoderSupport::FinishNoBuff(char * aDest, 
                                             PRInt32 * aDestLength)
{
  *aDestLength = 0;
  return NS_OK;
}

NS_IMETHODIMP nsEncoderSupport::FlushBuffer(char ** aDest, 
                                            const char * aDestEnd)
{
  PRInt32 bcr, bcw; // byte counts for read & write;
  nsresult res = NS_OK;
  char * dest = *aDest;

  if (mBufferStart < mBufferEnd) {
    bcr = mBufferEnd - mBufferStart;
    bcw = aDestEnd - dest;
    if (bcw < bcr) bcr = bcw;
    memcpy(dest, mBufferStart, bcr);
    dest += bcr;
    mBufferStart += bcr;

    if (mBufferStart < mBufferEnd) res = NS_OK_UENC_MOREOUTPUT;
  }

  *aDest = dest;
  return res;
}

//----------------------------------------------------------------------
// Interface nsISupports [implementation]

NS_IMPL_ADDREF(nsEncoderSupport);
NS_IMPL_RELEASE(nsEncoderSupport);

nsresult nsEncoderSupport::QueryInterface(REFNSIID aIID, 
                                           void** aInstancePtr)
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
                                                                         
  *aInstancePtr = NULL;                                                  
                                                                         
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 

  if (aIID.Equals(kIUnicodeEncoderIID)) {                                          
    *aInstancePtr = (void*) ((nsIUnicodeEncoder*)this); 
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*) ((nsISupports*)this);
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }                                                                      

  return NS_NOINTERFACE;                                                 
}

//----------------------------------------------------------------------
// Interface nsIUnicodeEncoder [implementation]

NS_IMETHODIMP nsEncoderSupport::Convert(const PRUnichar * aSrc, 
                                        PRInt32 * aSrcLength, 
                                        char * aDest, 
                                        PRInt32 * aDestLength)
{
  // we do all operations using pointers internally
  const PRUnichar * src = aSrc;
  const PRUnichar * srcEnd = aSrc + *aSrcLength;
  char * dest = aDest;
  char * destEnd = aDest + *aDestLength;

  PRInt32 bcr, bcw; // byte counts for read & write;
  nsresult res;

  res = FlushBuffer(&dest, destEnd);
  if (res == NS_OK_UENC_MOREOUTPUT) goto final;

  bcr = srcEnd - src;
  bcw = destEnd - dest;
  res = ConvertNoBuff(src, &bcr, dest, &bcw);
  src += bcr;
  dest += bcw;
  if ((res == NS_OK_UENC_MOREOUTPUT) && (dest < destEnd)) {
    // convert exactly one character into the internal buffer
    // at this point, there should be at least a char in the input
    for (;;) {
      bcr = 1;
      bcw = mBufferCapacity;
      res = ConvertNoBuff(src, &bcr, mBuffer, &bcw);

      if (res == NS_OK_UENC_MOREOUTPUT) {
        delete [] mBuffer;
        mBufferCapacity *= 2;
        mBuffer = new char [mBufferCapacity];
      } else {
        src += bcr;
        mBufferStart = mBufferEnd = mBuffer;
        mBufferEnd += bcw;
        break;
      }
    }

    res = FlushBuffer(&dest, destEnd);
  }

final:
  *aSrcLength   -= srcEnd - src;
  *aDestLength  -= destEnd - dest;
  return res;
}

NS_IMETHODIMP nsEncoderSupport::Finish(char * aDest, PRInt32 * aDestLength)
{
  // we do all operations using pointers internally
  char * dest = aDest;
  char * destEnd = aDest + *aDestLength;

  PRInt32 bcw; // byte count for write;
  nsresult res;

  res = FlushBuffer(&dest, destEnd);
  if (res == NS_OK_UENC_MOREOUTPUT) goto final;

  // do the finish into the internal buffer.
  for (;;) {
    bcw = mBufferCapacity;
    res = FinishNoBuff(mBuffer, &bcw);

    if (res == NS_OK_UENC_MOREOUTPUT) {
      delete [] mBuffer;
      mBufferCapacity *= 2;
      mBuffer = new char [mBufferCapacity];
    } else {
      mBufferStart = mBufferEnd = mBuffer;
      mBufferEnd += bcw;
      break;
    }
  }

  res = FlushBuffer(&dest, destEnd);

final:
  *aDestLength  -= destEnd - dest;
  return res;
}

NS_IMETHODIMP nsEncoderSupport::Reset()
{
  mBufferStart = mBufferEnd;
  return NS_OK;
}

NS_IMETHODIMP nsEncoderSupport::SetOutputErrorBehavior(
                                PRInt32 aBehavior, 
                                nsIUnicharEncoder * aEncoder, 
                                PRUnichar aChar)
{
  if ((aBehavior == kOnError_CallBack) && (aEncoder == NULL)) 
    return NS_ERROR_NULL_POINTER;

  NS_IF_RELEASE(aEncoder);
  mErrEncoder = aEncoder;
  NS_IF_ADDREF(aEncoder);

  mErrBehavior = aBehavior;
  mErrChar = aChar;
  return NS_OK;
}

//----------------------------------------------------------------------
// Class nsTableEncoderSupport [implementation]

nsTableEncoderSupport::nsTableEncoderSupport(uShiftTable * aShiftTable, 
                                             uMappingTable  * aMappingTable) 
: nsEncoderSupport()
{
  mHelper = NULL;
  mShiftTable = aShiftTable;
  mMappingTable = aMappingTable;
}

nsTableEncoderSupport::~nsTableEncoderSupport() 
{
  NS_IF_RELEASE(mHelper);
}

//----------------------------------------------------------------------
// Subclassing of nsEncoderSupport class [implementation]

NS_IMETHODIMP nsTableEncoderSupport::ConvertNoBuffNoErr(
                                     const PRUnichar * aSrc, 
                                     PRInt32 * aSrcLength, 
                                     char * aDest, 
                                     PRInt32 * aDestLength)
{
  nsresult res;

  if (mHelper == nsnull) {
    res = nsComponentManager::CreateInstance(kUnicodeEncodeHelperCID, NULL, 
        kIUnicodeEncodeHelperIID, (void**) & mHelper);
    
    if (NS_FAILED(res)) return NS_ERROR_UENC_NOHELPER;
  }

  res = mHelper->ConvertByTable(aSrc, aSrcLength, aDest, aDestLength, 
      mShiftTable, mMappingTable);
  return res;
}
