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
#include "unicpriv.h"
#include "nsIUnicodeEncoder.h"
#include "nsIUnicodeEncodeHelper.h"
#include "nsUnicodeEncodeHelper.h"
#include "nsUConvDll.h"

//----------------------------------------------------------------------
// Class nsUnicodeEncodeHelper [declaration]

/**
 * The actual implementation of the nsIUnicodeEncodeHelper interface.
 *
 * @created         22/Nov/1998
 * @author  Catalin Rotaru [CATA]
 */
class nsUnicodeEncodeHelper : public nsIUnicodeEncodeHelper
{
  NS_DECL_ISUPPORTS

public:

  /**
   * Class constructor.
   */
  nsUnicodeEncodeHelper();

  /**
   * Class destructor.
   */
  virtual ~nsUnicodeEncodeHelper();

  //--------------------------------------------------------------------
  // Interface nsIUnicodeEncodeHelper [declaration]

  NS_IMETHOD ConvertByTable(const PRUnichar * aSrc, PRInt32 * aSrcLength, 
      char * aDest, PRInt32 * aDestLength, uShiftTable * aShiftTable, 
      uMappingTable  * aMappingTable);

  NS_IMETHOD ConvertByTables(const PRUnichar * aSrc, PRInt32 * aSrcLength, 
      char * aDest, PRInt32 * aDestLength, PRInt32 aTableCount, 
      uShiftTable ** aShiftTable, uMappingTable  ** aMappingTable);

  NS_IMETHOD ConvertByMultiTable(const PRUnichar * aSrc, PRInt32 * aSrcLength,
      char * aDest, PRInt32 * aDestLength, PRInt32 aTableCount, 
      uShiftTable ** aShiftTable, uMappingTable  ** aMappingTable);
};

//----------------------------------------------------------------------
// Class nsUnicodeEncodeHelper [implementation]

NS_IMPL_ISUPPORTS(nsUnicodeEncodeHelper, kIUnicodeEncodeHelperIID);

nsUnicodeEncodeHelper::nsUnicodeEncodeHelper() 
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
}

nsUnicodeEncodeHelper::~nsUnicodeEncodeHelper() 
{
  PR_AtomicDecrement(&g_InstanceCount);
}

//----------------------------------------------------------------------
// Interface nsIUnicodeEncodeHelper [implementation]

NS_IMETHODIMP nsUnicodeEncodeHelper::ConvertByTable(const PRUnichar * aSrc, 
                                                    PRInt32 * aSrcLength, 
                                                    char * aDest, 
                                                    PRInt32 * aDestLength, 
                                                    uShiftTable * aShiftTable, 
                                                    uMappingTable  * aMappingTable)
{
  const PRUnichar * src = aSrc;
  const PRUnichar * srcEnd = aSrc + *aSrcLength;
  char * dest = aDest;
  PRInt32 destLen = *aDestLength;

  PRUnichar med;
  PRInt32 bcw; // byte count for write;
  nsresult res = NS_OK;

  while (src < srcEnd) {
    if (!uMapCode((uTable*) aMappingTable, *(src++), &med)) {
      res = NS_ERROR_UENC_NOMAPPING;
      break;
    }

    if (!uGenerate(aShiftTable, 0, med, (PRUint8 *)dest, destLen, 
      (PRUint32 *)&bcw)) { 
      src--;
      res = NS_OK_UENC_MOREOUTPUT;
      break;
    }

    dest += bcw;
    destLen -= bcw;
  }

  *aSrcLength = src - aSrc;
  *aDestLength  = dest - aDest;
  return res;
}

NS_IMETHODIMP nsUnicodeEncodeHelper::ConvertByTables(
                                     const PRUnichar * aSrc, 
                                     PRInt32 * aSrcLength, 
                                     char * aDest, 
                                     PRInt32 * aDestLength, 
                                     PRInt32 aTableCount, 
                                     uShiftTable ** aShiftTable, 
                                     uMappingTable  ** aMappingTable)
{
  // XXX deprecated
  return ConvertByMultiTable(aSrc, aSrcLength, aDest, aDestLength, aTableCount,
      aShiftTable, aMappingTable);
}

NS_IMETHODIMP nsUnicodeEncodeHelper::ConvertByMultiTable(
                                     const PRUnichar * aSrc, 
                                     PRInt32 * aSrcLength, 
                                     char * aDest, 
                                     PRInt32 * aDestLength, 
                                     PRInt32 aTableCount, 
                                     uShiftTable ** aShiftTable, 
                                     uMappingTable  ** aMappingTable)
{
  const PRUnichar * src = aSrc;
  const PRUnichar * srcEnd = aSrc + *aSrcLength;
  char * dest = aDest;
  PRInt32 destLen = *aDestLength;

  PRUnichar med;
  PRInt32 bcw; // byte count for write;
  nsresult res = NS_OK;
  PRInt32 i;

  while (src < srcEnd) {
    for (i=0; i<aTableCount; i++) 
      if (uMapCode((uTable*) aMappingTable[i], *src, &med)) break;

    src++;
    if (i == aTableCount) {
      res = NS_ERROR_UENC_NOMAPPING;
      break;
    }

    if (!uGenerate(aShiftTable[i], 0, med, (PRUint8 *)dest, destLen, 
      (PRUint32 *)&bcw)) { 
      src--;
      res = NS_OK_UENC_MOREOUTPUT;
      break;
    }

    dest += bcw;
    destLen -= bcw;
  }

  *aSrcLength = src - aSrc;
  *aDestLength  = dest - aDest;
  return res;
}

//----------------------------------------------------------------------
// Class nsEncodeHelperFactory [implementation]

NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
NS_IMPL_ISUPPORTS(nsEncodeHelperFactory, kIFactoryIID);

nsEncodeHelperFactory::nsEncodeHelperFactory() 
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
}

nsEncodeHelperFactory::~nsEncodeHelperFactory() 
{
  PR_AtomicDecrement(&g_InstanceCount);
}

//----------------------------------------------------------------------
// Interface nsIFactory [implementation]

NS_IMETHODIMP nsEncodeHelperFactory::CreateInstance(nsISupports *aDelegate, 
                                                    const nsIID &aIID,
                                                    void **aResult)
{
  if (aResult == NULL) return NS_ERROR_NULL_POINTER;
  if (aDelegate != NULL) return NS_ERROR_NO_AGGREGATION;

  nsIUnicodeEncodeHelper * t = new nsUnicodeEncodeHelper;
  if (t == NULL) return NS_ERROR_OUT_OF_MEMORY;
  
  nsresult res = t->QueryInterface(aIID, aResult);
  if (NS_FAILED(res)) delete t;

  return res;
}

NS_IMETHODIMP nsEncodeHelperFactory::LockFactory(PRBool aLock)
{
  if (aLock) PR_AtomicIncrement(&g_LockCount);
  else PR_AtomicDecrement(&g_LockCount);

  return NS_OK;
}
