/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsAreaFrame.h"
#include "nsBlockBandData.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsIViewManager.h"
#include "nsINodeInfo.h"
#include "nsHTMLAtoms.h"
#include "nsIView.h"
#include "nsHTMLValue.h"
#include "nsHTMLParts.h"
#include "nsLayoutAtoms.h"

#ifdef MOZ_XUL
#include "nsINameSpaceManager.h"
#include "nsXULAtoms.h"
#include "nsIEventStateManager.h"
#endif

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

#ifdef MOZ_XUL

// If you make changes to this function, check its counterparts 
// in nsBoxFrame and nsTextBoxFrame
nsresult
nsAreaFrame::RegUnregAccessKey(nsPresContext* aPresContext,
                               PRBool aDoReg)
{
  // if we have no content, we can't do anything
  if (!mContent)
    return NS_ERROR_FAILURE;

  nsINodeInfo *ni = mContent->GetNodeInfo();

  // only support accesskeys for the following elements
  if (!ni || !ni->Equals(nsXULAtoms::label, kNameSpaceID_XUL))
    return NS_OK;

  // To filter out <label>s without a control attribute.
  // XXXjag a side-effect is that we filter out anonymous <label>s
  // in e.g. <menu>, <menuitem>, <button>. These <label>s inherit
  // |accesskey| and would otherwise register themselves, overwriting
  // the content we really meant to be registered.
  if (!mContent->HasAttr(kNameSpaceID_None, nsXULAtoms::control))
    return NS_OK;

  nsAutoString accessKey;
  mContent->GetAttr(kNameSpaceID_None, nsXULAtoms::accesskey, accessKey);

  if (accessKey.IsEmpty())
    return NS_OK;

  // With a valid PresContext we can get the ESM 
  // and register the access key
  nsIEventStateManager *esm = aPresContext->EventStateManager();
  nsresult rv;

  PRUint32 key = accessKey.First();
  if (aDoReg)
    rv = esm->RegisterAccessKey(mContent, key);
  else
    rv = esm->UnregisterAccessKey(mContent, key);

  return rv;
}
#endif

/////////////////////////////////////////////////////////////////////////////
// nsIFrame

#ifdef MOZ_XUL
NS_IMETHODIMP
nsAreaFrame::Init(nsPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsStyleContext*  aContext,
                  nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsBlockFrame::Init(aPresContext,
                                   aContent,
                                   aParent,
                                   aContext,
                                   aPrevInFlow);
  if (NS_FAILED(rv))
    return rv;

  // register access key
  return RegUnregAccessKey(aPresContext, PR_TRUE);
}

NS_IMETHODIMP
nsAreaFrame::Destroy(nsPresContext* aPresContext)
{
  // unregister access key
  RegUnregAccessKey(aPresContext, PR_FALSE);

  return nsBlockFrame::Destroy(aPresContext);
} 

NS_IMETHODIMP
nsAreaFrame::AttributeChanged(nsPresContext* aPresContext,
                              nsIContent* aChild,
                              PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType)
{
  nsresult rv = nsBlockFrame::AttributeChanged(aPresContext, aChild,
                                               aNameSpaceID, aAttribute,
                                               aModType);

  // If the accesskey changed, register for the new value
  // The old value has been unregistered in nsXULElement::SetAttr
  if (aAttribute == nsXULAtoms::accesskey || aAttribute == nsXULAtoms::control)
    RegUnregAccessKey(aPresContext, PR_TRUE);

  return rv;
}
#endif

nsIAtom*
nsAreaFrame::GetType() const
{
  return nsLayoutAtoms::areaFrame;
}

/////////////////////////////////////////////////////////////////////////////
// Diagnostics

#ifdef NS_DEBUG
NS_IMETHODIMP
nsAreaFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Area"), aResult);
}
#endif
