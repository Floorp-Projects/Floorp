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

#include "nsIUnicodeDecoder.h"
#include "nsAscii2Unicode.h"
#include "nsConverterCID.h"
#include "nsUConvDll.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

#define NS_SRC_CHARSET  "Ascii"
#define NS_DEST_CHARSET "Unicode"

//----------------------------------------------------------------------
// Class nsAscii2Unicode [declaration]

/**
 * A character set converter from Ascii to Unicode.
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
class nsAscii2Unicode : public nsIUnicodeDecoder
{
  NS_DECL_ISUPPORTS

public:

  /**
   * Class constructor.
   */
  nsAscii2Unicode();

  /**
   * Class destructor.
   */
  ~nsAscii2Unicode();

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


};

//----------------------------------------------------------------------
// Class nsAscii2Unicode [implementation]

NS_IMPL_ISUPPORTS(nsAscii2Unicode, kIUnicodeDecoderIID);

nsAscii2Unicode::nsAscii2Unicode() 
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
}

nsAscii2Unicode::~nsAscii2Unicode() 
{
  PR_AtomicDecrement(&g_InstanceCount);
}

//----------------------------------------------------------------------
// Interface nsICharsetConverter [implementation]

NS_IMETHODIMP nsAscii2Unicode::Convert(PRUnichar * aDest, PRInt32 aDestOffset,
                                       PRInt32 * aDestLength, 
                                       const char * aSrc, PRInt32 aSrcOffset, 
                                       PRInt32 * aSrcLength)
{
  if (aDest == NULL) return NS_ERROR_NULL_POINTER;

  PRInt32 len = PR_MIN(*aSrcLength, *aDestLength);
  for (PRInt32 i=0; i<len; i++) *aDest++ = ((PRUint8)*aSrc++);

  nsresult res = (*aSrcLength > len)? NS_PARTIAL_MORE_OUTPUT : NS_OK;
  *aSrcLength = *aDestLength = len;

  return res;
}

NS_IMETHODIMP nsAscii2Unicode::Finish(PRUnichar * aDest, PRInt32 aDestOffset,
                                      PRInt32 * aDestLength)
{
  // it is really only a stateless converter...
  return NS_OK;
}

NS_IMETHODIMP nsAscii2Unicode::Length(const char * aSrc, PRInt32 aSrcOffset, 
                                      PRInt32 aSrcLength, 
                                      PRInt32 * aDestLength)
{
  // we are a single byte to Unicode converter, so...
  *aDestLength = aSrcLength;
  return NS_EXACT_LENGTH;
}

NS_IMETHODIMP nsAscii2Unicode::Reset()
{
  return NS_OK;
}

NS_IMETHODIMP nsAscii2Unicode::SetInputErrorBehavior(PRInt32 aBehavior)
{
  // no input error possible, this encoding is too simple
  return NS_OK;
}

//----------------------------------------------------------------------
// Class nsAscii2UnicodeFactory [implementation]

nsAscii2UnicodeFactory::nsAscii2UnicodeFactory() 
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
}

nsAscii2UnicodeFactory::~nsAscii2UnicodeFactory() 
{
  PR_AtomicDecrement(&g_InstanceCount);
}

//----------------------------------------------------------------------
// Interface nsISupports [implementation]

NS_IMPL_ADDREF(nsAscii2UnicodeFactory);
NS_IMPL_RELEASE(nsAscii2UnicodeFactory);

nsresult nsAscii2UnicodeFactory::QueryInterface(REFNSIID aIID, 
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

NS_IMETHODIMP nsAscii2UnicodeFactory::CreateInstance(nsISupports *aDelegate,
                                                const nsIID &aIID,
                                                void **aResult)
{
  if (aResult == NULL) return NS_ERROR_NULL_POINTER;
  if (aDelegate != NULL) return NS_ERROR_NO_AGGREGATION;

  nsIUnicodeDecoder * t = new nsAscii2Unicode;
  if (t == NULL) return NS_ERROR_OUT_OF_MEMORY;
  
  nsresult res = t->QueryInterface(aIID, aResult);
  if (NS_FAILED(res)) delete t;

  return res;
}

NS_IMETHODIMP nsAscii2UnicodeFactory::LockFactory(PRBool aLock)
{
  if (aLock) PR_AtomicIncrement(&g_LockCount);
  else PR_AtomicDecrement(&g_LockCount);

  return NS_OK;
}

//----------------------------------------------------------------------
// Interface nsICharsetConverterInfo [implementation]

NS_IMETHODIMP nsAscii2UnicodeFactory::GetCharsetSrc(nsString ** aCharset)
{
  (*aCharset) = new nsString(NS_SRC_CHARSET);
  return NS_OK;
}

NS_IMETHODIMP nsAscii2UnicodeFactory::GetCharsetDest(nsString ** aCharset)
{
  (*aCharset) = new nsString(NS_DEST_CHARSET);
  return NS_OK;
}
