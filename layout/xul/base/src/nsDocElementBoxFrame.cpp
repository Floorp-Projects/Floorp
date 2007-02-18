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
#include "nsHTMLParts.h"
#include "nsContainerFrame.h"
#include "nsCSSRendering.h"
#include "nsIDocument.h"
#include "nsPresContext.h"
#include "nsIWidget.h"
#include "nsPageFrame.h"
#include "nsIRenderingContext.h"
#include "nsGUIEvent.h"
#include "nsIDOMEvent.h"
#include "nsStyleConsts.h"
#include "nsGkAtoms.h"
#include "nsIEventStateManager.h"
#include "nsIDeviceContext.h"
#include "nsIPresShell.h"
#include "nsBoxFrame.h"
#include "nsStackLayout.h"
#include "nsIAnonymousContentCreator.h"
#include "nsINodeInfo.h"
#include "nsIServiceManager.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentUtils.h"

//#define DEBUG_REFLOW

class nsDocElementBoxFrame : public nsBoxFrame,
                             public nsIAnonymousContentCreator
{
public:
  virtual void Destroy();

  friend nsIFrame* NS_NewBoxFrame(nsIPresShell* aPresShell,
                                  nsStyleContext* aContext);

  nsDocElementBoxFrame(nsIPresShell* aShell, nsStyleContext* aContext)
    :nsBoxFrame(aShell, aContext, PR_TRUE) {}

  NS_DECL_ISUPPORTS_INHERITED

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(nsTArray<nsIContent*>& aElements);

  virtual PRBool IsFrameOfType(PRUint32 aFlags) const;
#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif
private:
  nsCOMPtr<nsIContent> mPopupgroupContent;
  nsCOMPtr<nsIContent> mTooltipContent;
};

//----------------------------------------------------------------------

nsIFrame*
NS_NewDocElementBoxFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsDocElementBoxFrame (aPresShell, aContext);
}

void
nsDocElementBoxFrame::Destroy()
{
  nsContentUtils::DestroyAnonymousContent(&mPopupgroupContent);
  nsContentUtils::DestroyAnonymousContent(&mTooltipContent);
  nsBoxFrame::Destroy();
}

nsresult
nsDocElementBoxFrame::CreateAnonymousContent(nsTArray<nsIContent*>& aElements)
{
  nsIDocument* doc = mContent->GetDocument();
  if (!doc) {
    // The page is currently being torn down.  Why bother.
    return NS_ERROR_FAILURE;
  }
  nsNodeInfoManager *nodeInfoManager = doc->NodeInfoManager();

  // create the top-secret popupgroup node. shhhhh!
  nsCOMPtr<nsINodeInfo> nodeInfo;
  nsresult rv = nodeInfoManager->GetNodeInfo(nsGkAtoms::popupgroup,
                                             nsnull, kNameSpaceID_XUL,
                                             getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewXULElement(getter_AddRefs(mPopupgroupContent), nodeInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aElements.AppendElement(mPopupgroupContent))
    return NS_ERROR_OUT_OF_MEMORY;

  // create the top-secret default tooltip node. shhhhh!
  rv = nodeInfoManager->GetNodeInfo(nsGkAtoms::tooltip, nsnull,
                                    kNameSpaceID_XUL, getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewXULElement(getter_AddRefs(mTooltipContent), nodeInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  mTooltipContent->SetAttr(nsnull, nsGkAtoms::_default,
                           NS_LITERAL_STRING("true"), PR_FALSE);

  if (!aElements.AppendElement(mTooltipContent))
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt) 
nsDocElementBoxFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt)
nsDocElementBoxFrame::Release(void)
{
  return NS_OK;
}

NS_INTERFACE_MAP_BEGIN(nsDocElementBoxFrame)
  NS_INTERFACE_MAP_ENTRY(nsIAnonymousContentCreator)
NS_INTERFACE_MAP_END_INHERITING(nsBoxFrame)

PRBool
nsDocElementBoxFrame::IsFrameOfType(PRUint32 aFlags) const
{
  // Override nsBoxFrame.
  return !aFlags;
}

#ifdef DEBUG
NS_IMETHODIMP
nsDocElementBoxFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("DocElementBox"), aResult);
}
#endif
