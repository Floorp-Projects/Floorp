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
 */
#include <stdio.h>
#include <ctype.h> /* isspace */

#include "nsCOMPtr.h"
#include "nsCtlCIID.h"
#include "nsILE.h"
#include "nsUnicodeToTIS620.h"

// XPCOM stuff
NS_IMPL_ADDREF(nsUnicodeToTIS620);
NS_IMPL_RELEASE(nsUnicodeToTIS620);

int 
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
  return (int)aRunList->numRuns;
}

nsresult nsUnicodeToTIS620::QueryInterface(REFNSIID aIID, void** aInstancePtr)
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

NS_IMETHODIMP nsUnicodeToTIS620::SetOutputErrorBehavior(PRInt32 aBehavior,
                                                        nsIUnicharEncoder * aEncoder, 
                                                        PRUnichar aChar)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

// constructor and destroctor

nsUnicodeToTIS620::nsUnicodeToTIS620()
{
  NS_INIT_REFCNT();
}

nsUnicodeToTIS620::~nsUnicodeToTIS620()
{
}

/*
 * This method converts the unicode to this font index.
 * Note: ConversionBufferFullException is not handled
 *       since this class is only used for character display.
 */
NS_IMETHODIMP nsUnicodeToTIS620::Convert(const PRUnichar * input, 
                                         PRInt32 * aSrcLength,
                                         char * output, PRInt32 * aDestLength)
{
  int             i = 0;
  nsCOMPtr<nsILE> mCtlObj;
  static          NS_DEFINE_CID(kLECID, NS_ULE_CID);
  nsresult        rv;
  textRunList     txtRuns;
  textRun         *aPtr, *aTmpPtr;

  // No question of starting the conversion from an offset
  charOff = byteOff = 0;

  mCtlObj = do_CreateInstance(kLECID, &rv);
  if (NS_FAILED(rv)) {
    printf("ERROR: Cannot create instance of component " NS_ULE_PROGID " [%x].\n", 
           rv);
    printf("Thai Text Layout Will Not Be Supported\n");
  }

  txtRuns.numRuns = 0;  
  Itemize(input, *aSrcLength, &txtRuns);

  aPtr = txtRuns.head;
  for (i = 0; i < txtRuns.numRuns; i++) {   
    // char    *tmpDestBuf = output + byteOff;
    // PRInt32 tmpDestLen = *aDestLength - byteOff;
    PRInt32 tmpSrcLen = aPtr->length;
    
    if (aPtr->isOther) {
      // PangoThaiShaper does not handle ASCII + thai in same shaper
      for (i = 0; i < tmpSrcLen; i++)
        output[i + byteOff] = (char)(*(aPtr->start + i));
      byteOff += tmpSrcLen;
    }
    else {
      PRSize outLen = *aDestLength;
      // Charset tis620-0, tis620.2533-1, tis620.2529-1 & iso8859-11
      // are equivalent and have the same presentation forms

      // tis620-2 is hard-coded since we only generate presentation forms
      // in Windows-Stye as it is the current defacto-standard for the
      // presentation of thai content
      mCtlObj->GetPresentationForm(aPtr->start, tmpSrcLen, "tis620-2",
                                   output, &outLen);
      byteOff += outLen;
    }
    aPtr = aPtr->next;
  }
  
  // Check for free nsILE or convert nsILE to a service
  // NS_IF_RELEASE(mCtlObj);

  // Cleanup Run Info;
  aPtr = txtRuns.head;
  for (int i = 0; i < txtRuns.numRuns; i++) {
    aTmpPtr = aPtr;
    aPtr = aPtr->next;
    delete aTmpPtr;
  }

  *aDestLength = byteOff;
  return NS_OK;
}

NS_IMETHODIMP nsUnicodeToTIS620::Finish(char * output, PRInt32 * aDestLength)
{
  // Finish does'nt have to do much as Convert already flushes
  // to output buffer
  byteOff = charOff = 0;
  return NS_OK;
}

//================================================================
NS_IMETHODIMP nsUnicodeToTIS620::Reset()
{
  byteOff = charOff = 0;
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
