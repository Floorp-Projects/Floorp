/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "pratom.h"
#include "nsIComponentManager.h"
#include "nsICharRepresentable.h"
#include "nsUCvTW2Support.h"
#include "nsUCvTW2Dll.h"

static NS_DEFINE_CID(kUnicodeDecodeHelperCID, NS_UNICODEDECODEHELPER_CID);
static NS_DEFINE_CID(kUnicodeEncodeHelperCID, NS_UNICODEENCODEHELPER_CID);

#define DEFAULT_BUFFER_CAPACITY 16

// XXX review the buffer growth limitation code

//----------------------------------------------------------------------
// Class nsBasicDecoderSupport [implementation]

nsBasicDecoderSupport::nsBasicDecoderSupport() 
{
  NS_INIT_REFCNT();
}

nsBasicDecoderSupport::~nsBasicDecoderSupport() 
{
}

//----------------------------------------------------------------------
// Interface nsISupports [implementation]

NS_IMPL_ADDREF(nsBasicDecoderSupport);
NS_IMPL_RELEASE(nsBasicDecoderSupport);

nsresult nsBasicDecoderSupport::QueryInterface(REFNSIID aIID, 
                                               void** aInstancePtr)
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
                                                                         
  *aInstancePtr = NULL;                                                  
                                                                         
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 

  if (aIID.Equals(NS_GET_IID(nsIUnicodeDecoder))) {                                          
    *aInstancePtr = (void*) ((nsIUnicodeDecoder*)this); 
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
// Interface nsIUnicodeDecoder [implementation]

//----------------------------------------------------------------------
// Class nsBufferDecoderSupport [implementation]

nsBufferDecoderSupport::nsBufferDecoderSupport() 
: nsBasicDecoderSupport()
{
  mBufferCapacity = DEFAULT_BUFFER_CAPACITY;
  mBuffer = new char[mBufferCapacity];

  Reset();
}

nsBufferDecoderSupport::~nsBufferDecoderSupport() 
{
  delete [] mBuffer;
}

void nsBufferDecoderSupport::FillBuffer(const char ** aSrc, PRInt32 aSrcLength)
{
  PRInt32 bcr = PR_MIN(mBufferCapacity - mBufferLength, aSrcLength);
  memcpy(mBuffer + mBufferLength, *aSrc, bcr);
  mBufferLength += bcr;
  (*aSrc) += bcr;
}

void nsBufferDecoderSupport::DoubleBuffer()
{
  mBufferCapacity *= 2;
  char * newBuffer = new char [mBufferCapacity];
  if (mBufferLength > 0) memcpy(newBuffer, mBuffer, mBufferLength);
  delete [] mBuffer;
  mBuffer = newBuffer;
}

//----------------------------------------------------------------------
// Subclassing of nsBasicDecoderSupport class [implementation]

NS_IMETHODIMP nsBufferDecoderSupport::Convert(const char * aSrc, 
                                              PRInt32 * aSrcLength,
                                              PRUnichar * aDest, 
                                              PRInt32 * aDestLength)
{
  // we do all operations using pointers internally
  const char * src = aSrc;
  const char * srcEnd = aSrc + *aSrcLength;
  PRUnichar * dest = aDest;
  PRUnichar * destEnd = aDest + *aDestLength;

  PRInt32 bcr, bcw; // byte counts for read & write;
  nsresult res = NS_OK;

  // do we have some residual data from the last conversion?
  if (mBufferLength > 0) if (dest == destEnd) {
    res = NS_OK_UDEC_MOREOUTPUT;
  } else for (;;) {
    // we need new data to add to the buffer
    if (src == srcEnd) {
      res = NS_OK_UDEC_MOREINPUT;
      break;
    }

    // fill that buffer
    PRInt32 buffLen = mBufferLength;  // initial buffer length
    FillBuffer(&src, srcEnd - src);

    // convert that buffer
    bcr = mBufferLength;
    bcw = destEnd - dest;
    res = ConvertNoBuff(mBuffer, &bcr, dest, &bcw);
    dest += bcw;

    if ((res == NS_OK_UDEC_MOREINPUT) && (bcw == 0)) {
        res = NS_ERROR_UNEXPECTED;
#if defined(DEBUG_yokoyama) || defined(DEBUG_ftang)
        NS_ASSERTION(0, "This should not happen. Internal buffer may be corrupted.");
#endif
        break;
    } else {
      if (bcr < buffLen) {
        // we didn't convert that residual data - unfill the buffer
        src -= mBufferLength - buffLen;
        mBufferLength = buffLen;
#if defined(DEBUG_yokoyama) || defined(DEBUG_ftang)
        NS_ASSERTION(0, "This should not happen. Internal buffer may be corrupted.");
#endif
      } else {
        // the buffer and some extra data was converted - unget the rest
        src -= mBufferLength - bcr;
        mBufferLength = 0;
        res = NS_OK;
      }
      break;
    }
  }

  if (res == NS_OK) {
    bcr = srcEnd - src;
    bcw = destEnd - dest;
    res = ConvertNoBuff(src, &bcr, dest, &bcw);
    src += bcr;
    dest += bcw;

    // if we have partial input, store it in our internal buffer.
    if (res == NS_OK_UDEC_MOREINPUT) {
      bcr = srcEnd - src;
      // make sure buffer is large enough
      if (bcr > mBufferCapacity) {
          // somehow we got into an error state and the buffer is growing out of control
          res = NS_ERROR_UNEXPECTED;
      } else {
          FillBuffer(&src, bcr);
      }
    }
  }

  *aSrcLength   -= srcEnd - src;
  *aDestLength  -= destEnd - dest;
  return res;
}

NS_IMETHODIMP nsBufferDecoderSupport::Reset()
{
  mBufferLength = 0;
  return NS_OK;
}

//----------------------------------------------------------------------
// Class nsTableDecoderSupport [implementation]

nsTableDecoderSupport::nsTableDecoderSupport(uShiftTable * aShiftTable, 
                                             uMappingTable  * aMappingTable) 
: nsBufferDecoderSupport()
{
  mHelper = NULL;
  mShiftTable = aShiftTable;
  mMappingTable = aMappingTable;
}

nsTableDecoderSupport::~nsTableDecoderSupport() 
{
  NS_IF_RELEASE(mHelper);
}

//----------------------------------------------------------------------
// Subclassing of nsBufferDecoderSupport class [implementation]

NS_IMETHODIMP nsTableDecoderSupport::ConvertNoBuff(const char * aSrc, 
                                                   PRInt32 * aSrcLength, 
                                                   PRUnichar * aDest, 
                                                   PRInt32 * aDestLength)
{
  nsresult res;

  if (mHelper == nsnull) {
    res = nsComponentManager::CreateInstance(kUnicodeDecodeHelperCID, NULL, 
        NS_GET_IID(nsIUnicodeDecodeHelper), (void**) & mHelper);
    
    if (NS_FAILED(res)) return NS_ERROR_UDEC_NOHELPER;
  }

  res = mHelper->ConvertByTable(aSrc, aSrcLength, aDest, aDestLength, 
      mShiftTable, mMappingTable);
  return res;
}

//----------------------------------------------------------------------
// Class nsMultiTableDecoderSupport [implementation]

nsMultiTableDecoderSupport::nsMultiTableDecoderSupport(
                            PRInt32 aTableCount,
                            uRange * aRangeArray, 
                            uShiftTable ** aShiftTable, 
                            uMappingTable ** aMappingTable) 
: nsBufferDecoderSupport()
{
  mHelper = NULL;
  mTableCount = aTableCount;
  mRangeArray = aRangeArray;
  mShiftTable = aShiftTable;
  mMappingTable = aMappingTable;
}

nsMultiTableDecoderSupport::~nsMultiTableDecoderSupport() 
{
  NS_IF_RELEASE(mHelper);
}

//----------------------------------------------------------------------
// Subclassing of nsBufferDecoderSupport class [implementation]

NS_IMETHODIMP nsMultiTableDecoderSupport::ConvertNoBuff(const char * aSrc, 
                                                        PRInt32 * aSrcLength, 
                                                        PRUnichar * aDest, 
                                                        PRInt32 * aDestLength)
{
  nsresult res;

  if (mHelper == nsnull) {
    res = nsComponentManager::CreateInstance(kUnicodeDecodeHelperCID, NULL, 
        NS_GET_IID(nsIUnicodeDecodeHelper), (void**) &mHelper);
    
    if (NS_FAILED(res)) return NS_ERROR_UDEC_NOHELPER;
  }

  res = mHelper->ConvertByMultiTable(aSrc, aSrcLength, aDest, aDestLength, 
      mTableCount, mRangeArray, mShiftTable, mMappingTable);
  return res;
}

//----------------------------------------------------------------------
// Class nsOneByteDecoderSupport [implementation]

nsOneByteDecoderSupport::nsOneByteDecoderSupport(
                         uShiftTable * aShiftTable, 
                         uMappingTable  * aMappingTable) 
: nsBasicDecoderSupport()
{
  mHelper = NULL;
  mShiftTable = aShiftTable;
  mMappingTable = aMappingTable;
}

nsOneByteDecoderSupport::~nsOneByteDecoderSupport() 
{
  NS_IF_RELEASE(mHelper);
}

//----------------------------------------------------------------------
// Subclassing of nsBasicDecoderSupport class [implementation]

NS_IMETHODIMP nsOneByteDecoderSupport::Convert(const char * aSrc, 
                                              PRInt32 * aSrcLength, 
                                              PRUnichar * aDest, 
                                              PRInt32 * aDestLength)
{
  nsresult res;

  if (mHelper == nsnull) {
    res = nsComponentManager::CreateInstance(kUnicodeDecodeHelperCID, NULL, 
        NS_GET_IID(nsIUnicodeDecodeHelper), (void**) &mHelper);
    if (NS_FAILED(res)) return NS_ERROR_UDEC_NOHELPER;

    res = mHelper -> CreateFastTable(mShiftTable, mMappingTable, mFastTable, 
        ONE_BYTE_TABLE_SIZE);
    if (NS_FAILED(res)) return res;
  }

  res = mHelper->ConvertByFastTable(aSrc, aSrcLength, aDest, aDestLength, 
      mFastTable, ONE_BYTE_TABLE_SIZE);
  return res;
}

NS_IMETHODIMP nsOneByteDecoderSupport::GetMaxLength(const char * aSrc, 
                                                    PRInt32 aSrcLength, 
                                                    PRInt32 * aDestLength)
{
  // single byte to Unicode converter
  *aDestLength = aSrcLength;
  return NS_OK_UDEC_EXACTLENGTH;
}

NS_IMETHODIMP nsOneByteDecoderSupport::Reset()
{
  // nothing to reset, no internal state in this case
  return NS_OK;
}

//----------------------------------------------------------------------
// Class nsBasicEncoder [implementation]
nsBasicEncoder::nsBasicEncoder() 
{
  NS_INIT_REFCNT();
}

nsBasicEncoder::~nsBasicEncoder() 
{
}

//----------------------------------------------------------------------
// Interface nsISupports [implementation]

NS_IMPL_ADDREF(nsBasicEncoder);
NS_IMPL_RELEASE(nsBasicEncoder);

nsresult nsBasicEncoder::QueryInterface(REFNSIID aIID, 
                                          void** aInstancePtr)
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
                                                                         
  *aInstancePtr = NULL;                                                  
                                                                         
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 

  if (aIID.Equals(NS_GET_IID(nsIUnicodeEncoder))) {                                          
    *aInstancePtr = (void*) ((nsIUnicodeEncoder*)this); 
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(NS_GET_IID(nsICharRepresentable))) {                                          
    *aInstancePtr = (void*) ((nsICharRepresentable*)this); 
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*) ((nsISupports*)((nsIUnicodeEncoder*)this));
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }                                                                      

  return NS_NOINTERFACE;                                                 
}
//----------------------------------------------------------------------
// Class nsEncoderSupport [implementation]

nsEncoderSupport::nsEncoderSupport() 
{
  mBufferCapacity = DEFAULT_BUFFER_CAPACITY;
  mBuffer = new char[mBufferCapacity];

  mErrBehavior = kOnError_Signal;
  mErrChar = 0;
  mErrEncoder = NULL;

  Reset();
}

nsEncoderSupport::~nsEncoderSupport() 
{
  delete [] mBuffer;
  NS_IF_RELEASE(mErrEncoder);
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

nsresult nsEncoderSupport::FlushBuffer(char ** aDest, const char * aDestEnd)
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
  mBufferStart = mBufferEnd = mBuffer;
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

NS_IMETHODIMP nsTableEncoderSupport::FillInfo(PRUint32 *aInfo) 
{
  nsresult res;

  if (mHelper == nsnull) {
    res = nsComponentManager::CreateInstance(kUnicodeEncodeHelperCID, NULL, 
        NS_GET_IID(nsIUnicodeEncodeHelper), (void**) & mHelper);
    
    if (NS_FAILED(res)) return NS_ERROR_UENC_NOHELPER;
  }

  res = mHelper->FillInfo(aInfo, mMappingTable);
  return res;
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
        NS_GET_IID(nsIUnicodeEncodeHelper), (void**) & mHelper);
    
    if (NS_FAILED(res)) return NS_ERROR_UENC_NOHELPER;
  }

  res = mHelper->ConvertByTable(aSrc, aSrcLength, aDest, aDestLength, 
      mShiftTable, mMappingTable);
  return res;
}

//----------------------------------------------------------------------
// Class nsMultiTableEncoderSupport [implementation]

nsMultiTableEncoderSupport::nsMultiTableEncoderSupport(
                            PRInt32 aTableCount,
                            uShiftTable ** aShiftTable, 
                            uMappingTable  ** aMappingTable) 
: nsEncoderSupport()
{
  mHelper = NULL;
  mTableCount = aTableCount;
  mShiftTable = aShiftTable;
  mMappingTable = aMappingTable;
}

nsMultiTableEncoderSupport::~nsMultiTableEncoderSupport() 
{
  NS_IF_RELEASE(mHelper);
}

NS_IMETHODIMP nsMultiTableEncoderSupport::FillInfo(PRUint32 *aInfo) 
{
  nsresult res;

  if (mHelper == nsnull) {
    res = nsComponentManager::CreateInstance(kUnicodeEncodeHelperCID, NULL, 
        NS_GET_IID(nsIUnicodeEncodeHelper), (void**) & mHelper);
    
    if (NS_FAILED(res)) return NS_ERROR_UENC_NOHELPER;
  }

  res = mHelper->FillInfo(aInfo,mTableCount, mMappingTable);
  return res;
}
//----------------------------------------------------------------------
// Subclassing of nsEncoderSupport class [implementation]

NS_IMETHODIMP nsMultiTableEncoderSupport::ConvertNoBuffNoErr(
                                          const PRUnichar * aSrc, 
                                          PRInt32 * aSrcLength, 
                                          char * aDest, 
                                          PRInt32 * aDestLength)
{
  nsresult res;

  if (mHelper == nsnull) {
    res = nsComponentManager::CreateInstance(kUnicodeEncodeHelperCID, NULL, 
        NS_GET_IID(nsIUnicodeEncodeHelper), (void**) & mHelper);
    
    if (NS_FAILED(res)) return NS_ERROR_UENC_NOHELPER;
  }

  res = mHelper->ConvertByMultiTable(aSrc, aSrcLength, aDest, aDestLength, 
      mTableCount, mShiftTable, mMappingTable);
  return res;
}
