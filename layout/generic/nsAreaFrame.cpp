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

/* derived class of nsBlockFrame; distinction barely relevant anymore */

#include "nsAreaFrame.h"
#include "nsBlockBandData.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsINodeInfo.h"
#include "nsGkAtoms.h"
#include "nsHTMLParts.h"

#ifdef MOZ_XUL
#include "nsINameSpaceManager.h"
#include "nsIEventStateManager.h"
#endif

#undef NOISY_MAX_ELEMENT_SIZE
#undef NOISY_FINAL_SIZE

nsIFrame*
NS_NewAreaFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, PRUint32 aFlags)
{
  nsAreaFrame* it = new (aPresShell) nsAreaFrame(aContext);
  
  if (it != nsnull)
    it->SetFlags(aFlags);

  return it;
}

#ifdef MOZ_XUL

// If you make changes to this function, check its counterparts 
// in nsBoxFrame and nsTextBoxFrame
nsresult
nsAreaFrame::RegUnregAccessKey(PRBool aDoReg)
{
  // if we have no content, we can't do anything
  if (!mContent)
    return NS_ERROR_FAILURE;

  // only support accesskeys for the following elements
  if (!mContent->NodeInfo()->Equals(nsGkAtoms::label, kNameSpaceID_XUL))
    return NS_OK;

  // To filter out <label>s without a control attribute.
  // XXXjag a side-effect is that we filter out anonymous <label>s
  // in e.g. <menu>, <menuitem>, <button>. These <label>s inherit
  // |accesskey| and would otherwise register themselves, overwriting
  // the content we really meant to be registered.
  if (!mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::control))
    return NS_OK;

  nsAutoString accessKey;
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::accesskey, accessKey);

  if (accessKey.IsEmpty())
    return NS_OK;

  // With a valid PresContext we can get the ESM 
  // and register the access key
  nsIEventStateManager *esm = PresContext()->EventStateManager();
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
nsAreaFrame::Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsBlockFrame::Init(aContent, aParent, aPrevInFlow);
  if (NS_FAILED(rv))
    return rv;

  // register access key
  return RegUnregAccessKey(PR_TRUE);
}

void
nsAreaFrame::Destroy()
{
  // unregister access key
  RegUnregAccessKey(PR_FALSE);
  nsBlockFrame::Destroy();
} 

NS_IMETHODIMP
nsAreaFrame::AttributeChanged(PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType)
{
  nsresult rv = nsBlockFrame::AttributeChanged(aNameSpaceID, 
                                               aAttribute, aModType);

  // If the accesskey changed, register for the new value
  // The old value has been unregistered in nsXULElement::SetAttr
  if (aAttribute == nsGkAtoms::accesskey || aAttribute == nsGkAtoms::control)
    RegUnregAccessKey(PR_TRUE);

  return rv;
}
#endif

nsIAtom*
nsAreaFrame::GetType() const
{
  return nsGkAtoms::areaFrame;
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
