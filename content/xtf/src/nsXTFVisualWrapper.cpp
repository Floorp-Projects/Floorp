/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla XTF project.
 *
 * The Initial Developer of the Original Code is 
 * Alex Fritze.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Alex Fritze <alex@croczilla.com> (original author)
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
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nsCOMPtr.h"
#include "nsXTFVisualWrapper.h"
#include "nsISupportsArray.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIDocument.h"

nsXTFVisualWrapper::nsXTFVisualWrapper(nsINodeInfo* aNodeInfo)
    : nsXTFVisualWrapperBase(aNodeInfo)
{}

//----------------------------------------------------------------------
// nsISupports implementation


NS_IMPL_ADDREF_INHERITED(nsXTFVisualWrapper,nsXTFVisualWrapperBase)
NS_IMPL_RELEASE_INHERITED(nsXTFVisualWrapper,nsXTFVisualWrapperBase)

NS_INTERFACE_MAP_BEGIN(nsXTFVisualWrapper)
  NS_INTERFACE_MAP_ENTRY(nsIXTFVisualWrapperPrivate)
NS_INTERFACE_MAP_END_INHERITING(nsXTFVisualWrapperBase)

//----------------------------------------------------------------------
// nsIXTFVisualWrapperPrivate implementation:

NS_IMETHODIMP
nsXTFVisualWrapper::CreateAnonymousContent(nsPresContext* aPresContext,
                                           nsISupportsArray& aAnonymousItems)
{
  nsIDocument *doc = GetCurrentDoc();
  NS_ASSERTION(doc, "no document; cannot create anonymous content");

  if (!mVisualContent) {
    GetXTFVisual()->GetVisualContent(getter_AddRefs(mVisualContent));
  }    
  if (!mVisualContent) return NS_OK; // nothing to append
  
  // Check if we are creating content for the primary presShell
  bool isPrimaryShell =
    (aPresContext->PresShell() == doc->GetShellAt(0));

  nsCOMPtr<nsIDOMNode> contentToAppend;
  if (!isPrimaryShell) {
    // The presShell we are generating content for is not a primary
    // shell. In practice that means it means we are constructing
    // content for printing. Instead of asking the xtf element to
    // construct new visual content, we just clone the exisiting
    // content. In this way, the xtf element only ever has to provide
    // one visual content tree which it can dynamically manipulate.
    mVisualContent->CloneNode(PR_TRUE, getter_AddRefs(contentToAppend));
  }
  else
    contentToAppend = mVisualContent;

  if (contentToAppend)
    aAnonymousItems.AppendElement(contentToAppend);
  
  return NS_OK;
}

void
nsXTFVisualWrapper::DidLayout()
{
  if (mNotificationMask & nsIXTFVisual::NOTIFY_DID_LAYOUT)
    GetXTFVisual()->DidLayout();
}

void
nsXTFVisualWrapper::GetInsertionPoint(nsIDOMElement** insertionPoint)
{
  GetXTFVisual()->GetInsertionPoint(insertionPoint);
}

PRBool
nsXTFVisualWrapper::ApplyDocumentStyleSheets()
{
  PRBool retval = PR_FALSE;
  GetXTFVisual()->GetApplyDocumentStyleSheets(&retval);
  return retval;
}
