/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ucvth : nsUnicodeToTIS620.h
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc.  Portions created by SUN are Copyright (C) 2000 SUN
 * Microsystems, Inc. All Rights Reserved.
 *
 * This module ucvth is based on the Thai Shaper in Pango by
 * Red Hat Software. Portions created by Redhat are Copyright (C) 
 * 1999 Red Hat Software.
 * 
 * Contributor(s):
 *   Prabhat Hegde (prabhat.hegde@sun.com)
 */

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsICharsetConverterManager2.h"
#include "nsILanguageAtomService.h"
#include "nsCtlCIID.h"
#include "nsILE.h"
#include "nsULE.h"
#include "nsUnicodeToTIS620.h"

static NS_DEFINE_CID(kCharSetManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

// XPCOM stuff
NS_IMPL_ADDREF(nsUnicodeToTIS620);
NS_IMPL_RELEASE(nsUnicodeToTIS620);

PRInt32
nsUnicodeToTIS620::Itemize(const PRUnichar* aSrcBuf, PRInt32 aSrcLen, textRunList *aRunList) 
{
  int            ct = 0, start = 0;
  PRBool         isTis = PR_FALSE;
  struct textRun *tmpChunk;

  // Handle Simple Case Now : Multiple Ranges later
  PRUnichar thaiBeg = 3585; // U+0x0E01;
  PRUnichar thaiEnd = 3675; // U+0x0E5b

  for (ct = 0; ct < aSrcLen;) {
    tmpChunk = new textRun;

    if (aRunList->numRuns == 0)
      aRunList->head = tmpChunk;
    else
      aRunList->cur->next = tmpChunk;
    aRunList->cur = tmpChunk;
    aRunList->numRuns++;
    
    tmpChunk->start = &aSrcBuf[ct];
    start = ct;
    isTis = (aSrcBuf[ct] >= thaiBeg && aSrcBuf[ct] <= thaiEnd);

    if (isTis) {
      while (isTis && ct < aSrcLen) {
        isTis = (aSrcBuf[ct] >= thaiBeg && aSrcBuf[ct] <= thaiEnd);
        if (isTis)
          ct++;
      }
      tmpChunk->isOther = PR_FALSE;
    }
    else {
      while (!isTis && ct < aSrcLen) {
        isTis = (aSrcBuf[ct] >= thaiBeg && aSrcBuf[ct] <= thaiEnd);
        if (!isTis)
          ct++;
      }
      tmpChunk->isOther = PR_TRUE;
    }
    
    tmpChunk->length = ct - start;
  }
  return (PRInt32)aRunList->numRuns;
}

nsresult nsUnicodeToTIS620::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr)
    return NS_ERROR_NULL_POINTER;

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

NS_IMETHODIMP nsUnicodeToTIS620::SetOutputErrorBehavior(PRInt32 aBehavior,
                                                        nsIUnicharEncoder * aEncoder, 
                                                        PRUnichar aChar)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

// constructor and destroctor

nsUnicodeToTIS620::nsUnicodeToTIS620()
{
  static   NS_DEFINE_CID(kLECID, NS_ULE_CID);
  nsresult rv;

  mCtlObj = do_CreateInstance(kLECID, &rv);
  if (NS_FAILED(rv)) {
#ifdef DEBUG_prabhat
    // No other error handling needed here since we 
    // handle absence of mCtlObj in Convert
    printf("ERROR: Cannot create instance of component " NS_ULE_PROGID " [%x].\n", 
           rv);
#endif
    NS_WARNING("Thai Text Layout Will Not Be Supported\n");
    mCtlObj = nsnull;
  }
}

nsUnicodeToTIS620::~nsUnicodeToTIS620()
{
  // Maybe convert nsILE to a service
  // No NS_IF_RELEASE(mCtlObj) of nsCOMPtr;
}

/*
 * This method converts the unicode to this font index.
 * Note: ConversionBufferFullException is not handled
 *       since this class is only used for character display.
 */
NS_IMETHODIMP nsUnicodeToTIS620::Convert(const PRUnichar* input,
                                         PRInt32*         aSrcLength,
                                         char*            output,
                                         PRInt32*         aDestLength)
{
  textRunList txtRuns;
  textRun     *aPtr, *aTmpPtr;
  int i;
  
#ifdef DEBUG_prabhath_no_shaper
  printf("Debug/Test Case of No thai pango shaper Object\n");
  // Comment out mCtlObj == nsnull for test purposes
#endif
  
  if (mCtlObj == nsnull) {
    nsICharsetConverterManager2* gCharSetManager = nsnull;
    nsIUnicodeEncoder*           gDefaultTISConverter = nsnull;
    nsresult                     res;
    nsServiceManager::GetService(kCharSetManagerCID,
      NS_GET_IID(nsICharsetConverterManager2), (nsISupports**) &gCharSetManager);

#ifdef DEBUG_prabhath
    printf("ERROR: No CTL IMPLEMENTATION - Default Thai Conversion");
    // CP874 is the default converter for thai ; 
    // In case mCtlObj is absent (no CTL support), use it to convert.
#endif

    if (!gCharSetManager)
      return NS_ERROR_FAILURE;
    
    nsCOMPtr<nsIAtom> charset = getter_AddRefs(NS_NewAtom("TIS-620"));   
    if (charset)
      res = gCharSetManager->GetUnicodeEncoder(charset, &gDefaultTISConverter);
    else {
      NS_IF_RELEASE(gCharSetManager);
      return NS_ERROR_FAILURE;
    }
    
    if (!gDefaultTISConverter) {
      NS_WARNING("cannot get default converter for tis-620");
      NS_IF_RELEASE(gCharSetManager);
      return NS_ERROR_FAILURE;
    }
    
    gDefaultTISConverter->Convert(input, aSrcLength, output, aDestLength);
    NS_IF_RELEASE(gCharSetManager);
    NS_IF_RELEASE(gDefaultTISConverter);
    return NS_OK;    
  }

  // CTLized shaping conversion starts here
  // No question of starting the conversion from an offset
  mCharOff = mByteOff = 0;

  txtRuns.numRuns = 0;
  Itemize(input, *aSrcLength, &txtRuns);

  aPtr = txtRuns.head;
  for (i = 0; i < txtRuns.numRuns; i++) {
    PRInt32 tmpSrcLen = aPtr->length;
    
    if (aPtr->isOther) {
      // PangoThaiShaper does not handle ASCII + thai in same shaper
      for (int j = 0; j < tmpSrcLen; j++)
        output[j + mByteOff] = (char)(*(aPtr->start + j));
      mByteOff += tmpSrcLen;
    }
    else {
      PRSize outLen = *aDestLength - mByteOff;
      // Charset tis620-0, tis620.2533-1, tis620.2529-1 & iso8859-11
      // are equivalent and have the same presentation forms

      // tis620-2 is hard-coded since we only generate presentation forms
      // in Windows-Stye as it is the current defacto-standard for the
      // presentation of thai content
      mCtlObj->GetPresentationForm(aPtr->start, tmpSrcLen, "tis620-2",
                                   &output[mByteOff], &outLen);
      mByteOff += outLen;
    }
    aPtr = aPtr->next;
  }
  
  // Cleanup Run Info;
  aPtr = txtRuns.head;
  for (i = 0; i < txtRuns.numRuns; i++) {
    aTmpPtr = aPtr;
    aPtr = aPtr->next;
    delete aTmpPtr;
  }

  *aDestLength = mByteOff;
  return NS_OK;
}

NS_IMETHODIMP nsUnicodeToTIS620::Finish(char * output, PRInt32 * aDestLength)
{
  // Finish does'nt have to do much as Convert already flushes
  // to output buffer
  mByteOff = mCharOff = 0;
  return NS_OK;
}

//================================================================
NS_IMETHODIMP nsUnicodeToTIS620::Reset()
{
  mByteOff = mCharOff = 0;
  return NS_OK;
}

//================================================================
NS_IMETHODIMP nsUnicodeToTIS620::GetMaxLength(const PRUnichar * aSrc, 
                                              PRInt32 aSrcLength,
                                              PRInt32 * aDestLength)
{
  *aDestLength = (aSrcLength + 1) *  2; // Each Thai character can generate
                                        // atmost two presentation forms
  return NS_OK;
}
//================================================================

NS_IMETHODIMP nsUnicodeToTIS620::FillInfo(PRUint32* aInfo)
{
  PRUint16 i;

  // 00-0x7f
  for (i = 0;i <= 0x7f; i++)
    SET_REPRESENTABLE(aInfo, i);

  // 0x0e01-0x0e7f
  for (i = 0x0e01; i <= 0xe3a; i++)    
    SET_REPRESENTABLE(aInfo, i);
   
  // U+0E3B - U+0E3E is undefined
  // U+0E3F - U+0E5B
  for (i = 0x0e3f; i <= 0x0e5b; i++)
    SET_REPRESENTABLE(aInfo, i);

  // U+0E5C - U+0E7F Undefined
  return NS_OK;
}
