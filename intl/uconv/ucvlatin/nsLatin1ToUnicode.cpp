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
#include "nsLatin1ToUnicode.h"
#include "nsConverterCID.h"
#include "nsUCvLatinDll.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

#define NS_SRC_CHARSET  "ISO-8859-1"
#define NS_DEST_CHARSET "Unicode"

//----------------------------------------------------------------------
// Class nsLatin1ToUnicode [declaration]

/**
 * A character set converter from Latin1 to Unicode.
 *
 * This particular converter does not use the general single-byte converter 
 * helper object. That is because someone may want to optimise this converter 
 * to the fullest, as it is one of the most heavily used.
 *
 * Multithreading: not an issue, the object has one instance per user thread.
 * As a plus, it is also stateless!
 * 
 * @created         23/Nov/1998
 * @author  Catalin Rotaru [CATA]
 */
class nsLatin1ToUnicode : public nsIUnicodeDecoder
{
  NS_DECL_ISUPPORTS

public:

  /**
   * Class constructor.
   */
  nsLatin1ToUnicode();

  /**
   * Class destructor.
   */
  ~nsLatin1ToUnicode();

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
// Class nsLatin1ToUnicode [implementation]

NS_IMPL_ISUPPORTS(nsLatin1ToUnicode, kIUnicodeDecoderIID);

nsLatin1ToUnicode::nsLatin1ToUnicode() 
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
}

nsLatin1ToUnicode::~nsLatin1ToUnicode() 
{
  PR_AtomicDecrement(&g_InstanceCount);
}

//----------------------------------------------------------------------
// Interface nsICharsetConverter [implementation]

NS_IMETHODIMP nsLatin1ToUnicode::Convert(PRUnichar * aDest, 
                                         PRInt32 aDestOffset,
                                         PRInt32 * aDestLength, 
                                         const char * aSrc, 
                                         PRInt32 aSrcOffset, 
                                         PRInt32 * aSrcLength)
{
  if (aDest == NULL) return NS_ERROR_NULL_POINTER;

  PRInt32 len = PR_MIN(*aSrcLength, *aDestLength);
  for (PRInt32 i=0; i<len; i++) *aDest++ = ((PRUint8)*aSrc++);

  nsresult res = (*aSrcLength > len)? NS_PARTIAL_MORE_OUTPUT : NS_OK;
  *aSrcLength = *aDestLength = len;

  return res;
}

NS_IMETHODIMP nsLatin1ToUnicode::Finish(PRUnichar * aDest, 
                                        PRInt32 aDestOffset,
                                        PRInt32 * aDestLength)
{
  // it is really only a stateless converter...
  return NS_OK;
}

NS_IMETHODIMP nsLatin1ToUnicode::Length(const char * aSrc, 
                                        PRInt32 aSrcOffset, 
                                        PRInt32 aSrcLength, 
                                        PRInt32 * aDestLength)
{
  // we are a single byte to Unicode converter, so...
  *aDestLength = aSrcLength;
  return NS_EXACT_LENGTH;
}

NS_IMETHODIMP nsLatin1ToUnicode::Reset()
{
  return NS_OK;
}

NS_IMETHODIMP nsLatin1ToUnicode::SetInputErrorBehavior(PRInt32 aBehavior)
{
  // no input error possible, this encoding is too simple
  return NS_OK;
}

//----------------------------------------------------------------------
// Class nsLatin1ToUnicodeFactory [implementation]

nsLatin1ToUnicodeFactory::nsLatin1ToUnicodeFactory() 
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
}

nsLatin1ToUnicodeFactory::~nsLatin1ToUnicodeFactory() 
{
  PR_AtomicDecrement(&g_InstanceCount);
}

//----------------------------------------------------------------------
// Interface nsISupports [implementation]

NS_IMPL_ADDREF(nsLatin1ToUnicodeFactory);
NS_IMPL_RELEASE(nsLatin1ToUnicodeFactory);

nsresult nsLatin1ToUnicodeFactory::QueryInterface(REFNSIID aIID, 
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

NS_IMETHODIMP nsLatin1ToUnicodeFactory::CreateInstance(nsISupports *aDelegate,
                                                       const nsIID &aIID,
                                                       void **aResult)
{
  if (aResult == NULL) return NS_ERROR_NULL_POINTER;
  if (aDelegate != NULL) return NS_ERROR_NO_AGGREGATION;

  nsIUnicodeDecoder * t = new nsLatin1ToUnicode;
  if (t == NULL) return NS_ERROR_OUT_OF_MEMORY;
  
  nsresult res = t->QueryInterface(aIID, aResult);
  if (NS_FAILED(res)) delete t;

  return res;
}

NS_IMETHODIMP nsLatin1ToUnicodeFactory::LockFactory(PRBool aLock)
{
  if (aLock) PR_AtomicIncrement(&g_LockCount);
  else PR_AtomicDecrement(&g_LockCount);

  return NS_OK;
}

//----------------------------------------------------------------------
// Interface nsICharsetConverterInfo [implementation]

NS_IMETHODIMP nsLatin1ToUnicodeFactory::GetCharsetSrc(char ** aCharset)
{
  (*aCharset) = NS_SRC_CHARSET;
  return NS_OK;
}

NS_IMETHODIMP nsLatin1ToUnicodeFactory::GetCharsetDest(char ** aCharset)
{
  (*aCharset) = NS_DEST_CHARSET;
  return NS_OK;
}
