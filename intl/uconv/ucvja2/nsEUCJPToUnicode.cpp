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

#include "nsRepository.h"
#include "nsIUnicodeDecoder.h"
#include "nsIUnicodeDecodeUtil.h"
#include "nsEUCJPToUnicode.h"
#include "nsICharsetConverterManager.h"
#include "nsUCVJA2Dll.h"
#include "nsUCVJA2CID.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

#define NS_SRC_CHARSET  "EUC-JP"
#define NS_DEST_CHARSET "Unicode"

//----------------------------------------------------------------------
// Class nsEUCJPToUnicode [declaration]

class nsEUCJPToUnicode : public nsIUnicodeDecoder
{
  NS_DECL_ISUPPORTS

public:

  /**
   * Class constructor.
   */
  nsEUCJPToUnicode();

  /**
   * Class destructor.
   */
  ~nsEUCJPToUnicode();

  //--------------------------------------------------------------------
  // Interface nsIUnicodeDecoder [declaration]

  NS_IMETHOD Convert(PRUnichar * aDest, PRInt32 aDestOffset, 
      PRInt32 * aDestLength,const char * aSrc, PRInt32 aSrcOffset, 
      PRInt32 * aSrcLength);
  NS_IMETHOD Finish(PRUnichar * aDest, PRInt32 aDestOffset, 
      PRInt32 * aDestLength);
  NS_IMETHOD Length(const char * aSrc, PRInt32 aSrcOffset, PRInt32 aSrcLength, 
      PRInt32 * aDestLength);
  NS_IMETHOD Reset();
  NS_IMETHOD SetInputErrorBehavior(PRInt32 aBehavior);

private:
  PRInt32 mBehavior;
  nsIUnicodeDecodeUtil *mUtil;

};

// Shift Table
static PRInt16 g0201ShiftTable[] =  {
        2, uMultibytesCharset,
        ShiftCell(u1ByteChar,           1, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x7F),
        ShiftCell(u1BytePrefix8EChar, 2, 0x8E, 0x8E, 0x00, 0xA1, 0x00, 0xDF)
};
static PRInt16 g0208ShiftTable[] =  {
        0, u2BytesGRCharset,
        ShiftCell(0,0,0,0,0,0,0,0)
};

static PRInt16 g0212ShiftTable[] =  {
        0, u2BytesGRPrefix8EA2Charset,
        ShiftCell(0,0,0,0,0,0,0,0)
};
static PRInt16 *gShiftTables[4] =  {
    g0208ShiftTable,
    g0201ShiftTable,
    g0201ShiftTable,
    g0212ShiftTable
};

static PRUint16 *gMappingTables[4] = {
    g_0208Mapping,
    g_0201Mapping,
    g_0201Mapping,
    g_0212Mapping
};

static uRange gRanges[] = {
    { 0xA1, 0xFE },
    { 0x00, 0x7F },
    { 0x8E, 0x8E },
    { 0x8F, 0x8F } 
};


//----------------------------------------------------------------------
// Class nsEUCJPToUnicode [implementation]

NS_IMPL_ISUPPORTS(nsEUCJPToUnicode, kIUnicodeDecoderIID);

nsEUCJPToUnicode::nsEUCJPToUnicode() 
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
  mUtil = nsnull;
  mBehavior = kOnError_Recover;
}

nsEUCJPToUnicode::~nsEUCJPToUnicode() 
{
  NS_IF_RELEASE(mUtil);
  PR_AtomicDecrement(&g_InstanceCount);
}


//----------------------------------------------------------------------
// Interface nsICharsetConverter [implementation]

NS_IMETHODIMP nsEUCJPToUnicode::Convert(PRUnichar * aDest, PRInt32 aDestOffset,
                                       PRInt32 * aDestLength, 
                                       const char * aSrc, PRInt32 aSrcOffset, 
                                       PRInt32 * aSrcLength)
{
  if (aDest == NULL) return NS_ERROR_NULL_POINTER;
  if(nsnull == mUtil)
  {
     nsresult res = NS_OK;
     res = nsRepository::CreateInstance(
             kCharsetConverterManagerCID, 
             NULL,
             kIUnicodeDecodeUtilIID,
             (void**) & mUtil);
    
     if(NS_FAILED(res))
     {
       *aSrcLength = 0;
       *aDestLength = 0;
       return res;
     }
  }
  return mUtil->Convert( aDest, aDestOffset, aDestLength, 
                                 aSrc, aSrcOffset, aSrcLength,
                                 mBehavior,
				 4,
				 (uRange*) gRanges,
                                 (uShiftTable*) gShiftTables, 			
                                 (uMappingTable*)gMappingTables);
}

NS_IMETHODIMP nsEUCJPToUnicode::Finish(PRUnichar * aDest, PRInt32 aDestOffset,
                                      PRInt32 * aDestLength)
{
  // it is really only a stateless converter...
  *aDestLength = 0;
  return NS_OK;
}

NS_IMETHODIMP nsEUCJPToUnicode::Length(const char * aSrc, PRInt32 aSrcOffset, 
                                      PRInt32 aSrcLength, 
                                      PRInt32 * aDestLength)
{
  *aDestLength = aSrcLength;
  return NS_OK;
}

NS_IMETHODIMP nsEUCJPToUnicode::Reset()
{
  return NS_OK;
}

NS_IMETHODIMP nsEUCJPToUnicode::SetInputErrorBehavior(PRInt32 aBehavior)
{
  mBehavior = aBehavior;
  return NS_OK;
}

//----------------------------------------------------------------------
// Class nsEUCJPToUnicodeFactory [implementation]

nsEUCJPToUnicodeFactory::nsEUCJPToUnicodeFactory() 
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
}

nsEUCJPToUnicodeFactory::~nsEUCJPToUnicodeFactory() 
{
  PR_AtomicDecrement(&g_InstanceCount);
}

//----------------------------------------------------------------------
// Interface nsISupports [implementation]

NS_IMPL_ADDREF(nsEUCJPToUnicodeFactory);
NS_IMPL_RELEASE(nsEUCJPToUnicodeFactory);

nsresult nsEUCJPToUnicodeFactory::QueryInterface(REFNSIID aIID, 
                                                void** aInstancePtr)
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
                                                                         
  *aInstancePtr = NULL;                                                  
                                                                         
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kICharsetConverterInfoIID);                         
  static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);

  if (aIID.Equals(kClassIID)) {                                          
    *aInstancePtr = (void*) ((nsICharsetConverterInfo*)this); 
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kIFactoryIID)) {                                          
    *aInstancePtr = (void*) ((nsIFactory*)this); 
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*) ((nsISupports*)(nsIFactory*)this);
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }                                                                      

  return NS_NOINTERFACE;                                                 
}

//----------------------------------------------------------------------
// Interface nsIFactory [implementation]

NS_IMETHODIMP nsEUCJPToUnicodeFactory::CreateInstance(nsISupports *aDelegate,
                                                const nsIID &aIID,
                                                void **aResult)
{
  if (aResult == NULL) return NS_ERROR_NULL_POINTER;
  if (aDelegate != NULL) return NS_ERROR_NO_AGGREGATION;

  nsIUnicodeDecoder * t = new nsEUCJPToUnicode;
  if (t == NULL) return NS_ERROR_OUT_OF_MEMORY;
  
  nsresult res = t->QueryInterface(aIID, aResult);
  if (NS_FAILED(res)) delete t;

  return res;
}

NS_IMETHODIMP nsEUCJPToUnicodeFactory::LockFactory(PRBool aLock)
{
  if (aLock) PR_AtomicIncrement(&g_LockCount);
  else PR_AtomicDecrement(&g_LockCount);

  return NS_OK;
}

//----------------------------------------------------------------------
// Interface nsICharsetConverterInfo [implementation]

NS_IMETHODIMP nsEUCJPToUnicodeFactory::GetCharsetSrc(char ** aCharset)
{
  (*aCharset) = NS_SRC_CHARSET;
  return NS_OK;
}

NS_IMETHODIMP nsEUCJPToUnicodeFactory::GetCharsetDest(char ** aCharset)
{
  (*aCharset) = NS_DEST_CHARSET;
  return NS_OK;
}
