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
#include "nsISO88597ToUnicode.h"
#include "nsICharsetConverterManager.h"
#include "ns1ByteToUnicodeBase.h"
#include "nsUCvLatinDll.h"
#include "nsUCvLatinCID.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

#define NS_SRC_CHARSET  "ISO-8859-7"
#define NS_DEST_CHARSET "Unicode"

//----------------------------------------------------------------------
// Class nsISO88597ToUnicode [declaration]

class nsISO88597ToUnicode : public ns1ByteToUnicodeBase
{
  NS_DECL_ISUPPORTS

public:

  /**
   * Class constructor.
   */
  nsISO88597ToUnicode();

  /**
   * Class destructor.
   */
  ~nsISO88597ToUnicode();

protected:
  virtual uMappingTable* GetMappingTable();
  virtual PRUnichar* GetFastTable();
  virtual PRBool GetFastTableInitState();
  virtual void SetFastTableInit();

};


static PRUint16 gMappingTable[] = {
#include "8859-7.ut"
};

static PRBool gFastTableInit = PR_FALSE;
static PRUnichar gFastTable [256] ;

//----------------------------------------------------------------------
// Class nsISO88597ToUnicode [implementation]

NS_IMPL_ISUPPORTS(nsISO88597ToUnicode, kIUnicodeDecoderIID);

nsISO88597ToUnicode::nsISO88597ToUnicode() 
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
}

nsISO88597ToUnicode::~nsISO88597ToUnicode() 
{
  PR_AtomicDecrement(&g_InstanceCount);
}

uMappingTable* nsISO88597ToUnicode::GetMappingTable() 
{
  return (uMappingTable*) &gMappingTable;
}

PRUnichar * nsISO88597ToUnicode::GetFastTable() 
{
  return gFastTable;
}

PRBool nsISO88597ToUnicode::GetFastTableInitState() 
{
  return gFastTableInit;
}

void nsISO88597ToUnicode::SetFastTableInit() 
{
  gFastTableInit = PR_TRUE;
}
//----------------------------------------------------------------------
// Class nsISO88597ToUnicodeFactory [implementation]

nsISO88597ToUnicodeFactory::nsISO88597ToUnicodeFactory() 
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
}

nsISO88597ToUnicodeFactory::~nsISO88597ToUnicodeFactory() 
{
  PR_AtomicDecrement(&g_InstanceCount);
}

//----------------------------------------------------------------------
// Interface nsISupports [implementation]

NS_IMPL_ADDREF(nsISO88597ToUnicodeFactory);
NS_IMPL_RELEASE(nsISO88597ToUnicodeFactory);

nsresult nsISO88597ToUnicodeFactory::QueryInterface(REFNSIID aIID, 
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

NS_IMETHODIMP nsISO88597ToUnicodeFactory::CreateInstance(nsISupports *aDelegate,
                                                const nsIID &aIID,
                                                void **aResult)
{
  if (aResult == NULL) return NS_ERROR_NULL_POINTER;
  if (aDelegate != NULL) return NS_ERROR_NO_AGGREGATION;

  nsIUnicodeDecoder * t = new nsISO88597ToUnicode;
  if (t == NULL) return NS_ERROR_OUT_OF_MEMORY;
  
  nsresult res = t->QueryInterface(aIID, aResult);
  if (NS_FAILED(res)) delete t;

  return res;
}

NS_IMETHODIMP nsISO88597ToUnicodeFactory::LockFactory(PRBool aLock)
{
  if (aLock) PR_AtomicIncrement(&g_LockCount);
  else PR_AtomicDecrement(&g_LockCount);

  return NS_OK;
}

//----------------------------------------------------------------------
// Interface nsICharsetConverterInfo [implementation]

NS_IMETHODIMP nsISO88597ToUnicodeFactory::GetCharsetSrc(char ** aCharset)
{
  (*aCharset) = NS_SRC_CHARSET;
  return NS_OK;
}

NS_IMETHODIMP nsISO88597ToUnicodeFactory::GetCharsetDest(char ** aCharset)
{
  (*aCharset) = NS_DEST_CHARSET;
  return NS_OK;
}
