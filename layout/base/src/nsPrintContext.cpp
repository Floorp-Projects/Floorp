/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsPresContext.h"
#include "nsIPrintContext.h"
#include "nsIDeviceContext.h"
#include "nsUnitConversion.h"
#include "nsIView.h"
#include "nsIWidget.h"
#include "nsGfxCIID.h"
#include "nsLayoutAtoms.h"
#include "nsIPrintSettings.h"
#include "imgIContainer.h"


class PrintContext : public nsPresContext , nsIPrintContext{
public:
  //NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPRINTCONTEXT_IID)

//Interfaces for addref and release and queryinterface
//NOTE macro used is for classes that inherit from 
// another class. Only the base class should use NS_DECL_ISUPPORTS
  NS_DECL_ISUPPORTS_INHERITED


  PrintContext();
  ~PrintContext();

  virtual void SetPaginatedScrolling(PRBool aResult) {}
  virtual void GetPageDim(nsRect* aActualRect, nsRect* aAdjRect);
  virtual void SetPageDim(nsRect* aRect);
  virtual void SetImageAnimationMode(PRUint16 aMode);
  NS_IMETHOD SetPrintSettings(nsIPrintSettings* aPS);
  NS_IMETHOD GetPrintSettings(nsIPrintSettings** aPS);

protected:
  nsRect       mPageDim;
  nsCOMPtr<nsIPrintSettings> mPrintSettings;
};

PrintContext::PrintContext() :
  mPageDim(0,0,0,0)
{
  SetBackgroundImageDraw(PR_FALSE);
  SetBackgroundColorDraw(PR_FALSE);
  // Printed images are never animated
  mImageAnimationMode = imgIContainer::kDontAnimMode;
  mNeverAnimate = PR_TRUE;
  mMedium = nsLayoutAtoms::print;
  mPaginated = PR_TRUE;
}

PrintContext::~PrintContext()
{
}

NS_IMPL_ADDREF_INHERITED(PrintContext,nsPresContext)


NS_IMPL_RELEASE_INHERITED(PrintContext,nsPresContext)

//---------------------------------------------------------
NS_IMETHODIMP
PrintContext::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
 
  if (aIID.Equals(NS_GET_IID(nsIPrintContext))) {
    *aInstancePtr = (void *)((nsIPrintContext*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return nsPresContext::QueryInterface(aIID, aInstancePtr);
}

void
PrintContext::GetPageDim(nsRect* aActualRect, nsRect* aAdjRect)
{
  if (aActualRect && aAdjRect) {

    PRInt32 width,height;
    nsresult rv = mDeviceContext->GetDeviceSurfaceDimensions(width, height);
    if (NS_SUCCEEDED(rv))
      aActualRect->SetRect(0, 0, width, height);
  }
  *aAdjRect = mPageDim;
}

void
PrintContext::SetPageDim(nsRect* aPageDim)
{
  if (aPageDim)
    mPageDim = *aPageDim;
}

/**
 * Ignore any attempt to set an animation mode for printed images
 */
void
PrintContext::SetImageAnimationMode(PRUint16 aMode)
{
}

NS_IMETHODIMP 
PrintContext::SetPrintSettings(nsIPrintSettings * aPrintSettings)
{
  NS_ENSURE_ARG_POINTER(aPrintSettings);
  mPrintSettings = aPrintSettings;
  return NS_OK;
}

NS_IMETHODIMP 
PrintContext::GetPrintSettings(nsIPrintSettings * *aPrintSettings)
{
  NS_ENSURE_ARG_POINTER(aPrintSettings);

  *aPrintSettings = mPrintSettings;
  NS_IF_ADDREF(*aPrintSettings);

  return NS_OK;
}


nsresult
NS_NewPrintContext(nsIPrintContext** aInstancePtrResult)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  if (aInstancePtrResult == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

  PrintContext *it = new PrintContext;

  if (it == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(NS_GET_IID(nsIPrintContext), (void **) aInstancePtrResult);
}
