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

// YY need to pass isMultiple before create called

#include "nsTitleFrame.h"

/*
#include "nsTitleFrame.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsISupports.h"
#include "nsIAtom.h"
#include "nsIHTMLContent.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLParts.h"
#include "nsHTMLAtoms.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsStyleUtil.h"
#include "nsFont.h"
#include "nsFormControlFrame.h"
*/

static NS_DEFINE_IID(kTitleFrameCID, NS_TITLE_FRAME_CID);
 
nsresult
NS_NewTitleFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsTitleFrame* it = new (aPresShell) nsTitleFrame(aPresShell);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsTitleFrame::nsTitleFrame(nsIPresShell* aPresShell):nsBoxFrame(aPresShell)
{
}

nsTitleFrame::~nsTitleFrame()
{
}

// Frames are not refcounted, no need to AddRef
NS_IMETHODIMP
nsTitleFrame::QueryInterface(REFNSIID aIID, void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kTitleFrameCID)) {
    *aInstancePtrResult = (void*) ((nsTitleFrame*)this);
    return NS_OK;
  }
  return nsBoxFrame::QueryInterface(aIID, aInstancePtrResult);
}



#ifdef NS_DEBUG
NS_IMETHODIMP
nsTitleFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Title", aResult);
}
#endif
