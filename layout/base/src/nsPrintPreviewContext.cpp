/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsPresContext.h"
#include "nsIDeviceContext.h"
#include "nsUnitConversion.h"
#include "nsIView.h"
#include "nsIWidget.h"
#include "nsGfxCIID.h"
#include "nsLayoutAtoms.h"
#include "prlog.h"

static NS_DEFINE_IID(kIPresContextIID, NS_IPRESCONTEXT_IID);

class PrintPreviewContext : public nsPresContext {
public:
  PrintPreviewContext();
  ~PrintPreviewContext();

  NS_IMETHOD GetMedium(nsIAtom** aMedium);
  NS_IMETHOD IsPaginated(PRBool* aResult);
  NS_IMETHOD GetPageWidth(nscoord* aResult);
  NS_IMETHOD GetPageHeight(nscoord* aResult);

#ifdef NS_DEBUG
  static PRBool UseFakePageSize();
#endif
};

#ifdef NS_DEBUG
PRBool
PrintPreviewContext::UseFakePageSize()
{
  static PRLogModuleInfo* pageSizeLM;
  static PRBool useFakePageSize = PR_FALSE;
  if (nsnull == pageSizeLM) {
    pageSizeLM = PR_NewLogModule("pagesize");
    if (nsnull != pageSizeLM) {
      useFakePageSize = 0 != pageSizeLM->level;
    }
  }
  return useFakePageSize;
}
#endif

PrintPreviewContext::PrintPreviewContext()
{
}

PrintPreviewContext::~PrintPreviewContext()
{
}

NS_IMETHODIMP
PrintPreviewContext::GetMedium(nsIAtom** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = nsLayoutAtoms::print;
  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
PrintPreviewContext::IsPaginated(PRBool* aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
PrintPreviewContext::GetPageWidth(nscoord* aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }

#ifdef NS_DEBUG
  if (UseFakePageSize()) {
    // For testing purposes make the page width smaller than the visible
    // area
    float sbWidth, sbHeight;
    mDeviceContext->GetScrollBarDimensions(sbWidth, sbHeight);
    nscoord sbar = NSToCoordRound(sbWidth);
    *aResult = mVisibleArea.width - sbar - 2*100;
    return NS_OK;
  }
#endif

  // XXX assumes a 1/2 margin around all sides
  *aResult = (nscoord) NS_INCHES_TO_TWIPS(7.5);
  return NS_OK;
}

NS_IMETHODIMP
PrintPreviewContext::GetPageHeight(nscoord* aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }

#ifdef NS_DEBUG
  if (UseFakePageSize()) {
    // For testing purposes make the page height 60% of the visible area
    *aResult = mVisibleArea.height * 60 / 100;
    return NS_OK;
  }
#endif

  // XXX assumes a 1/2 margin around all sides
  *aResult = (nscoord) NS_INCHES_TO_TWIPS(10);
  return NS_OK;
}

NS_LAYOUT nsresult
NS_NewPrintPreviewContext(nsIPresContext** aInstancePtrResult)
{
  if (aInstancePtrResult == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

  PrintPreviewContext *it = new PrintPreviewContext();

  if (it == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kIPresContextIID, (void **) aInstancePtrResult);
}
