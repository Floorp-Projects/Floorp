/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ucvhi : nsUnicodeToSunIndic.h
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
 * Contributor(s):
 *   Prabhat Hegde (prabhat.hegde@sun.com)
 */

#include "nsCOMPtr.h"
#include "nsCtlCIID.h"
#include "nsILE.h"
#include "nsULE.h"
#include "nsUnicodeToSunIndic.h"

NS_IMPL_ADDREF(nsUnicodeToSunIndic);
NS_IMPL_RELEASE(nsUnicodeToSunIndic);

PRInt32
nsUnicodeToSunIndic::Itemize(const PRUnichar* aSrcBuf, PRInt32 aSrcLen, textRunList *aRunList) 
{
  int            ct = 0, start = 0;
  PRBool         isHindi = PR_FALSE;
  struct textRun *tmpChunk;

  // Handle Simple Case Now : Multiple Ranges later
  PRUnichar hindiBeg = 2305; // U+0x0901;
  PRUnichar hindiEnd = 2416; // U+0x097f;

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
    isHindi = (aSrcBuf[ct] >= hindiBeg && aSrcBuf[ct] <= hindiEnd);

    if (isHindi) {
      while (isHindi && ct < aSrcLen) {
        isHindi = (aSrcBuf[ct] >= hindiBeg && aSrcBuf[ct] <= hindiEnd);
        if (isHindi)
          ct++;
      }
      tmpChunk->isOther = PR_FALSE;
    }
    else {
      while (!isHindi && ct < aSrcLen) {
        isHindi = (aSrcBuf[ct] >= hindiBeg && aSrcBuf[ct] <= hindiEnd);
        if (!isHindi)
          ct++;
      }
      tmpChunk->isOther = PR_TRUE;
    }
    
    tmpChunk->length = ct - start;
  }
  return (PRInt32)aRunList->numRuns;
}

nsresult nsUnicodeToSunIndic::QueryInterface(REFNSIID aIID, void** aInstancePtr)
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

NS_IMETHODIMP nsUnicodeToSunIndic::SetOutputErrorBehavior(PRInt32 aBehavior,
                                                          nsIUnicharEncoder * aEncoder, 
                                                          PRUnichar aChar)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

// constructor and destructor

nsUnicodeToSunIndic::nsUnicodeToSunIndic()
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

    NS_WARNING("Indian Text Shaping Will Not Be Supported\n");
    mCtlObj = nsnull;
  }
}

nsUnicodeToSunIndic::~nsUnicodeToSunIndic()
{
  // Maybe convert nsILE to a service
  // No NS_IF_RELEASE(mCtlObj) of nsCOMPtr;
}

/*
 * This method converts the unicode to this font index.
 * Note: ConversionBufferFullException is not handled
 *       since this class is only used for character display.
 */
NS_IMETHODIMP nsUnicodeToSunIndic::Convert(const PRUnichar* input,
                                           PRInt32*         aSrcLength,
                                           char*            output,
                                           PRInt32*         aDestLength)
{
  textRunList txtRuns;
  textRun     *aPtr, *aTmpPtr;
  int i;  
  
  if (mCtlObj == nsnull) {
    nsresult res;

#ifdef DEBUG_prabhath
  printf("Debug/Test Case of No Hindi pango shaper Object\n");
  // Comment out mCtlObj == nsnull for test purposes
  printf("ERROR: No Hindi Text Layout Implementation");
#endif

    NS_WARNING("cannot get default converter for Hindi");    
    return NS_ERROR_FAILURE;
  }

  mCharOff = mByteOff = 0;

  txtRuns.numRuns = 0;
  Itemize(input, *aSrcLength, &txtRuns);

  aPtr = txtRuns.head;
  for (i = 0; i < txtRuns.numRuns; i++) {
    PRInt32 tmpSrcLen = aPtr->length;
    
    if (aPtr->isOther) {
      // PangoHindiShaper does not handle ASCII + Hindi in same shaper
      for (int j = 0; j < tmpSrcLen; j++)
        output[j + mByteOff] = (char)(*(aPtr->start + j));
      mByteOff += tmpSrcLen;
    }
    else {
      PRSize outLen = *aDestLength - mByteOff;
      // At the moment only generate presentation forms for
      // sun.unicode.india are supported.
      mCtlObj->GetPresentationForm(aPtr->start, tmpSrcLen, "sun.unicode.india-0",
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

NS_IMETHODIMP nsUnicodeToSunIndic::Finish(char *output, PRInt32 *aDestLength)
{
  // Finish does'nt have to do much as Convert already flushes
  // to output buffer
  mByteOff = mCharOff = 0;
  return NS_OK;
}

//================================================================
NS_IMETHODIMP nsUnicodeToSunIndic::Reset()
{
  mByteOff = mCharOff = 0;
  return NS_OK;
}

//================================================================
NS_IMETHODIMP nsUnicodeToSunIndic::GetMaxLength(const PRUnichar * aSrc, 
                                                PRInt32 aSrcLength,
                                                PRInt32 * aDestLength)
{
  *aDestLength = (aSrcLength + 1) *  2; // Each Hindi character can generate
                                        // atmost two presentation forms
  return NS_OK;
}
//================================================================

NS_IMETHODIMP nsUnicodeToSunIndic::FillInfo(PRUint32* aInfo)
{
  PRUint16 i;

  // 00-0x7f
  for (i = 0;i <= 0x7f; i++)
    SET_REPRESENTABLE(aInfo, i);

  // \u904, \u90a, \u93b, \u94e, \u94f are Undefined
  for (i = 0x0901; i <= 0x0903; i++)
    SET_REPRESENTABLE(aInfo, i);

  for (i = 0x0905; i <= 0x0939; i++)
    SET_REPRESENTABLE(aInfo, i);

  for (i = 0x093c; i <= 0x094d; i++)
    SET_REPRESENTABLE(aInfo, i);

  for (i = 0x0950; i <= 0x0954; i++)
    SET_REPRESENTABLE(aInfo, i);

  for (i = 0x0958; i <= 0x0970; i++)
    SET_REPRESENTABLE(aInfo, i); 
 
  // ZWJ and ZWNJ support & coverage need to be added.
  return NS_OK;
}

