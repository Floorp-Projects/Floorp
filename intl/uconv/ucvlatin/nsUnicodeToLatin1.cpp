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
#include "nsUCvLatinSupport.h"
#include "nsUnicodeToLatin1.h"
#include "nsUCvLatinCID.h"
#include "nsUCvLatinDll.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

#define NS_SRC_CHARSET  "Unicode"
#define NS_DEST_CHARSET "ISO-8859-1"

static PRUint16 g_Latin1MappingTable[] = {
#include "8859-1.uf"
};

static PRInt16 g_Latin1ShiftTable[] =  {
  1, u1ByteCharset ,
  ShiftCell(0,0,0,0,0,0,0,0)
};

//----------------------------------------------------------------------
// Class nsUnicodeToLatin1 [declaration]

/**
 * A character set converter from Unicode to Latin1.
 *
 * @created         17/Feb/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsUnicodeToLatin1 : public nsTableEncoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsUnicodeToLatin1();

protected:

  //--------------------------------------------------------------------
  // Subclassing of nsEncoderSupport class [declaration]

  NS_IMETHOD GetMaxLength(const PRUnichar * aSrc, PRInt32 aSrcLength, 
      PRInt32 * aDestLength);
};

//----------------------------------------------------------------------
// Class nsUnicodeToLatin1 [implementation]

nsUnicodeToLatin1::nsUnicodeToLatin1() 
: nsTableEncoderSupport((uShiftTable*) &g_Latin1ShiftTable, 
                        (uMappingTable*) &g_Latin1MappingTable)
{
}

//----------------------------------------------------------------------
// Subclassing of nsTableEncoderSupport class [implementation]

NS_IMETHODIMP nsUnicodeToLatin1::GetMaxLength(const PRUnichar * aSrc, 
                                              PRInt32 aSrcLength,
                                              PRInt32 * aDestLength)
{
  *aDestLength = aSrcLength;
  return NS_OK_UENC_EXACTLENGTH;
}

//----------------------------------------------------------------------
// Class nsUnicodeToLatin1Factory [implementation]

nsUnicodeToLatin1Factory::nsUnicodeToLatin1Factory() 
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
}

nsUnicodeToLatin1Factory::~nsUnicodeToLatin1Factory() 
{
  PR_AtomicDecrement(&g_InstanceCount);
}

//----------------------------------------------------------------------
// Interface nsISupports [implementation]

NS_IMPL_ADDREF(nsUnicodeToLatin1Factory);
NS_IMPL_RELEASE(nsUnicodeToLatin1Factory);

nsresult nsUnicodeToLatin1Factory::QueryInterface(REFNSIID aIID, 
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

NS_IMETHODIMP nsUnicodeToLatin1Factory::CreateInstance(nsISupports *aDelegate,
                                                       const nsIID &aIID,
                                                       void **aResult)
{
  if (aResult == NULL) return NS_ERROR_NULL_POINTER;
  if (aDelegate != NULL) return NS_ERROR_NO_AGGREGATION;

  nsIUnicodeEncoder * t = new nsUnicodeToLatin1;
  if (t == NULL) return NS_ERROR_OUT_OF_MEMORY;
  
  nsresult res = t->QueryInterface(aIID, aResult);
  if (NS_FAILED(res)) delete t;

  return res;
}

NS_IMETHODIMP nsUnicodeToLatin1Factory::LockFactory(PRBool aLock)
{
  if (aLock) PR_AtomicIncrement(&g_LockCount);
  else PR_AtomicDecrement(&g_LockCount);

  return NS_OK;
}

//----------------------------------------------------------------------
// Interface nsICharsetConverterInfo [implementation]

NS_IMETHODIMP nsUnicodeToLatin1Factory::GetCharsetSrc(char ** aCharset)
{
  (*aCharset) = NS_SRC_CHARSET;
  return NS_OK;
}

NS_IMETHODIMP nsUnicodeToLatin1Factory::GetCharsetDest(char ** aCharset)
{
  (*aCharset) = NS_DEST_CHARSET;
  return NS_OK;
}
