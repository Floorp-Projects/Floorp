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

#define NS_IMPL_IDS

#include "nsRepository.h"
#include "nsIUnicodeDecoder.h"
#include "nsIUnicodeDecodeUtil.h"
#include "nsSJIS2Unicode.h"
#include "nsICharsetConverterManager.h"
#include "nsUCVJADll.h"
#include "nsUCVJACID.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

#define NS_SRC_CHARSET  "Shift_JIS"
#define NS_DEST_CHARSET "Unicode"

//----------------------------------------------------------------------
// Class nsSJIS2Unicode [declaration]

/**
 * A character set converter from SJIS to Unicode.
 *
 * This particular converter does not use the general single-byte converter 
 * helper object. That is because someone may want to optimise this converter 
 * to the fullest, as it is the most heavily used one.
 *
 * Multithreading: not an issue, the object has one instance per user thread.
 * As a plus, it is also stateless!
 * 
 * @created         23/Nov/1998
 * @author  Catalin Rotaru [CATA]
 */
class nsSJIS2Unicode : public nsIUnicodeDecoder
{
  NS_DECL_ISUPPORTS

public:

  /**
   * Class constructor.
   */
  nsSJIS2Unicode();

  /**
   * Class destructor.
   */
  ~nsSJIS2Unicode();

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

static PRInt16 gShiftTable[] =  {
          4, uMultibytesCharset,
        ShiftCell(u1ByteChar,   1, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x7F),
        ShiftCell(u1ByteChar,   1, 0xA1, 0xDF, 0x00, 0xA1, 0x00, 0xDF),
        ShiftCell(u2BytesChar,  2, 0x81, 0x9F, 0x81, 0x40, 0x9F, 0xFC),
        ShiftCell(u2BytesChar,  2, 0xE0, 0xFC, 0xE0, 0x40, 0xFC, 0xFC)
};

static PRUint16 gMappingTable[] = {
#include "sjis.ut"
};


//----------------------------------------------------------------------
// Class nsSJIS2Unicode [implementation]

NS_IMPL_ISUPPORTS(nsSJIS2Unicode, kIUnicodeDecoderIID);

nsSJIS2Unicode::nsSJIS2Unicode() 
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
  mUtil = nsnull;
  mBehavior = kOnError_Recover;
}

nsSJIS2Unicode::~nsSJIS2Unicode() 
{
  NS_IF_RELEASE(mUtil);
  PR_AtomicDecrement(&g_InstanceCount);
}


//----------------------------------------------------------------------
// Interface nsICharsetConverter [implementation]

NS_IMETHODIMP nsSJIS2Unicode::Convert(PRUnichar * aDest, PRInt32 aDestOffset,
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
                                 (uShiftTable*) &gShiftTable, 
                                 (uMappingTable*)&gMappingTable);
}

NS_IMETHODIMP nsSJIS2Unicode::Finish(PRUnichar * aDest, PRInt32 aDestOffset,
                                      PRInt32 * aDestLength)
{
  // it is really only a stateless converter...
  *aDestLength = 0;
  return NS_OK;
}

NS_IMETHODIMP nsSJIS2Unicode::Length(const char * aSrc, PRInt32 aSrcOffset, 
                                      PRInt32 aSrcLength, 
                                      PRInt32 * aDestLength)
{
  *aDestLength = aSrcLength;
  return NS_OK;
}

NS_IMETHODIMP nsSJIS2Unicode::Reset()
{
  return NS_OK;
}

NS_IMETHODIMP nsSJIS2Unicode::SetInputErrorBehavior(PRInt32 aBehavior)
{
  mBehavior = aBehavior;
  return NS_OK;
}

//----------------------------------------------------------------------
// Class nsSJIS2UnicodeFactory [implementation]

nsSJIS2UnicodeFactory::nsSJIS2UnicodeFactory() 
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
}

nsSJIS2UnicodeFactory::~nsSJIS2UnicodeFactory() 
{
  PR_AtomicDecrement(&g_InstanceCount);
}

//----------------------------------------------------------------------
// Interface nsISupports [implementation]

NS_IMPL_ADDREF(nsSJIS2UnicodeFactory);
NS_IMPL_RELEASE(nsSJIS2UnicodeFactory);

nsresult nsSJIS2UnicodeFactory::QueryInterface(REFNSIID aIID, 
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

NS_IMETHODIMP nsSJIS2UnicodeFactory::CreateInstance(nsISupports *aDelegate,
                                                const nsIID &aIID,
                                                void **aResult)
{
  if (aResult == NULL) return NS_ERROR_NULL_POINTER;
  if (aDelegate != NULL) return NS_ERROR_NO_AGGREGATION;

  nsIUnicodeDecoder * t = new nsSJIS2Unicode;
  if (t == NULL) return NS_ERROR_OUT_OF_MEMORY;
  
  nsresult res = t->QueryInterface(aIID, aResult);
  if (NS_FAILED(res)) delete t;

  return res;
}

NS_IMETHODIMP nsSJIS2UnicodeFactory::LockFactory(PRBool aLock)
{
  if (aLock) PR_AtomicIncrement(&g_LockCount);
  else PR_AtomicDecrement(&g_LockCount);

  return NS_OK;
}

//----------------------------------------------------------------------
// Interface nsICharsetConverterInfo [implementation]

NS_IMETHODIMP nsSJIS2UnicodeFactory::GetCharsetSrc(char ** aCharset)
{
  (*aCharset) = NS_SRC_CHARSET;
  return NS_OK;
}

NS_IMETHODIMP nsSJIS2UnicodeFactory::GetCharsetDest(char ** aCharset)
{
  (*aCharset) = NS_DEST_CHARSET;
  return NS_OK;
}
