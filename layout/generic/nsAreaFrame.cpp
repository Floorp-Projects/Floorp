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
#include "nsAreaFrame.h"
#include "nsBlockBandData.h"
#include "nsIReflowCommand.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIViewManager.h"
#include "nsHTMLAtoms.h"
#include "nsIView.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLValue.h"
#include "nsHTMLParts.h"
#include "nsLayoutAtoms.h"
#include "nsISizeOfHandler.h"

#undef NOISY_MAX_ELEMENT_SIZE
#undef NOISY_FINAL_SIZE

nsresult
NS_NewAreaFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRUint32 aFlags)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsAreaFrame* it = new (aPresShell) nsAreaFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  it->SetFlags(aFlags);
  *aNewFrame = it;
  return NS_OK;
}

nsAreaFrame::nsAreaFrame()
{
}

/////////////////////////////////////////////////////////////////////////////
// nsIFrame

NS_IMETHODIMP
nsAreaFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::areaFrame; 
  NS_ADDREF(*aType);
  return NS_OK;
}

/////////////////////////////////////////////////////////////////////////////
// Diagnostics

#ifdef NS_DEBUG
NS_IMETHODIMP
nsAreaFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Area", aResult);
}

NS_IMETHODIMP
nsAreaFrame::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsBlockFrame::SizeOf(aHandler, aResult);
  *aResult += sizeof(*this) - sizeof(nsBlockFrame);

  return NS_OK;
}
#endif
