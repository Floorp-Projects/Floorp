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
 * The Original Code is XForms code.
 *
 * The Initial Developer of the Original Code is
 * Novell, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Allan Beaufour <abeaufour@novell.com>
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

#include "nsIDOMEvent.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNodeList.h"
#include "nsIXFormsRepeatElement.h"
#include "nsIXFormsControl.h"

#include "nsString.h"

#include "nsIInstanceElementPrivate.h"
#include "nsXFormsActionModuleBase.h"
#include "nsXFormsActionElement.h"
#include "nsXFormsUtils.h"
#include "nsIDOM3Node.h"

#include "math.h"

#ifdef DEBUG
//#define DEBUG_XF_INSERTDELETE
#endif

/**
 * Implementation of the XForms \<insert\> and \<delete\> elements.
 *
 * @see http://www.w3.org/TR/xforms/slice9.html#action-insert
 *
 * @todo The spec. states that the events must set their Context Info to:
 * "Path expression used for insert/delete (xsd:string)" (XXX)
 * @see http://www.w3.org/TR/xforms/slice4.html#evt-insert
 * @see https://bugzilla.mozilla.org/show_bug.cgi?id=280423
 *
 */
class nsXFormsInsertDeleteElement : public nsXFormsActionModuleBase
{
private:
  PRBool mIsInsert;

  nsresult RefreshRepeats(nsIDOMNode *aNode);

public:
  NS_DECL_NSIXFORMSACTIONMODULEELEMENT

  /** Constructor */
  nsXFormsInsertDeleteElement(PRBool aIsInsert) :
    mIsInsert(aIsInsert)
    {}
};

NS_IMETHODIMP
nsXFormsInsertDeleteElement::HandleAction(nsIDOMEvent            *aEvent,
                                          nsIXFormsActionElement *aParentAction)
{
  if (!mElement)
    return NS_OK;

  // First we need to get the three attributes: @nodeset, @at, and possibly
  // @position

  //
  // 1) Get @nodeset
  nsresult rv;
  nsCOMPtr<nsIModelElementPrivate> model;
  nsCOMPtr<nsIDOMXPathResult> nodeset;
  PRBool usesModelBinding;
  rv = nsXFormsUtils::EvaluateNodeBinding(mElement,
                                          nsXFormsUtils::ELEMENT_WITH_MODEL_ATTR,
                                          NS_LITERAL_STRING("nodeset"),
                                          EmptyString(),
                                          nsIDOMXPathResult::ORDERED_NODE_SNAPSHOT_TYPE,
                                          getter_AddRefs(model),
                                          getter_AddRefs(nodeset),
                                          &usesModelBinding);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!model || !nodeset)
    return NS_OK;

  PRUint32 setSize;
  rv = nodeset->GetSnapshotLength(&setSize);
  NS_ENSURE_SUCCESS(rv, rv);

  // Unfortunately we cannot insert into an empty set, as there's nothing to
  // clone.
  if (setSize < 1)
    return NS_OK;

  //
  // 2) Get @at
  nsAutoString atExpr;
  mElement->GetAttribute(NS_LITERAL_STRING("at"), atExpr);
  if (atExpr.IsEmpty())
    return NS_OK;

  // Context node is first node in nodeset
  nsCOMPtr<nsIDOMNode> contextNode;
  nodeset->SnapshotItem(0, getter_AddRefs(contextNode));

  nsCOMPtr<nsIDOMXPathResult> at;
  rv = nsXFormsUtils::EvaluateXPath(atExpr, contextNode, mElement,
                                    nsIDOMXPathResult::NUMBER_TYPE,
                                    getter_AddRefs(at), 1, setSize);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!at)
    return NS_OK;

  PRUint32 atInt = 0;
  double atDoub;
  rv = at->GetNumberValue(&atDoub);
  NS_ENSURE_SUCCESS(rv, rv);

  /// XXX we need to check for NaN, and select last row. but isnan() is not
  /// portable :(
  if (atDoub < 1) {
    atInt = 1;
  } else {
    atInt = (PRInt32) floor(atDoub + 0.5);
    if (atInt > setSize)
      atInt = setSize;
  }
  

  //
  // 3) Get @position (if \<insert\>)
  nsAutoString position;
  PRBool insertAfter = PR_FALSE;
  if (mIsInsert) {
    mElement->GetAttribute(NS_LITERAL_STRING("position"), position);
    if (position.EqualsLiteral("after")) {
      insertAfter = PR_TRUE;
    } else if (!position.EqualsLiteral("before")) {
      // This is not a valid document...
      return NS_ERROR_ABORT;
    }
  }
  
#ifdef DEBUG_XF_INSERTDELETE
  if (mIsInsert) {
    printf("Will try to INSERT node _%s_ index %d (set size: %d)\n",
           NS_ConvertUTF16toUTF8(position).get(),
           atInt,
           setSize);
  } else {
    printf("Will try to DELETE node at index %d (set size: %d)\n",
           atInt,
           setSize);
  }  
#endif

  // Find location
  nsCOMPtr<nsIDOMNode> location;
  nodeset->SnapshotItem(atInt - 1, getter_AddRefs(location));
  NS_ENSURE_STATE(location);

  nsCOMPtr<nsIDOMNode> parent;
  location->GetParentNode(getter_AddRefs(parent));
  NS_ENSURE_STATE(parent);

  if (mIsInsert && insertAfter) {
    nsCOMPtr<nsIDOMNode> temp;
    // If we're at the end of the nodeset, this returns nsnull, which is fine,
    // because InsertBefore then inserts at the end of the nodeset
    location->GetNextSibling(getter_AddRefs(temp));
    location.swap(temp);
  }

  nsCOMPtr<nsIDOMNode> resNode;
  if (mIsInsert) {
    // Get prototype and clone it (last member of nodeset)
    nsCOMPtr<nsIDOMNode> prototype;
    nodeset->SnapshotItem(setSize - 1, getter_AddRefs(prototype));
    NS_ENSURE_STATE(prototype);  

    nsCOMPtr<nsIDOMNode> newNode;
    prototype->CloneNode(PR_TRUE, getter_AddRefs(newNode));
    NS_ENSURE_STATE(newNode);

    parent->InsertBefore(newNode,
                         location,
                         getter_AddRefs(resNode));
    NS_ENSURE_STATE(resNode);

    // Set indexes for repeats
    rv = RefreshRepeats(resNode);
    NS_ENSURE_SUCCESS(rv, rv);

  } else {
    rv = parent->RemoveChild(location, getter_AddRefs(resNode));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Dispatch xforms-insert/delete event to the instance node we have modified
  // data for
  nsCOMPtr<nsIDOMNode> instNode;
  rv = nsXFormsUtils::GetInstanceNodeForData(resNode, getter_AddRefs(instNode));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = nsXFormsUtils::DispatchEvent(instNode,
                                    mIsInsert ? eEvent_Insert : eEvent_Delete);
  NS_ENSURE_SUCCESS(rv, rv);

  // Dispatch refreshing events to the model
  if (aParentAction) {
    aParentAction->SetRebuild(model, PR_TRUE);
    aParentAction->SetRecalculate(model, PR_TRUE);
    aParentAction->SetRevalidate(model, PR_TRUE);
    aParentAction->SetRefresh(model, PR_TRUE);
  } else {
    rv = model->RequestRebuild();
    NS_ENSURE_SUCCESS(rv, rv);
    rv = model->RequestRecalculate();
    NS_ENSURE_SUCCESS(rv, rv);
    rv = model->RequestRevalidate();
    NS_ENSURE_SUCCESS(rv, rv);
    rv = model->RequestRefresh();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


nsresult
nsXFormsInsertDeleteElement::RefreshRepeats(nsIDOMNode *aNode)
{
  // XXXbeaufour: only check repeats belonging to the same model...
  // possibly use mFormControls? Should be quicker than searching through
  // entire document!! mModel->GetControls("repeat"); Would also possibly
  // save a QI?

  nsCOMPtr<nsIDOMDocument> document;

  nsresult rv = mElement->GetOwnerDocument(getter_AddRefs(document));
  NS_ENSURE_STATE(document);

  nsCOMPtr<nsIDOMNodeList> repeatNodes;
  document->GetElementsByTagNameNS(NS_LITERAL_STRING(NS_NAMESPACE_XFORMS),
                                   NS_LITERAL_STRING("repeat"),
                                   getter_AddRefs(repeatNodes));
  NS_ENSURE_STATE(repeatNodes);

  // work over each node and if the node contains the inserted element
  PRUint32 nodeCount;
  rv = repeatNodes->GetLength(&nodeCount);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRUint32 node = 0; node < nodeCount; ++node) {
    nsCOMPtr<nsIDOMNode> repeatNode;

    rv = repeatNodes->Item(node, getter_AddRefs(repeatNode));
    nsCOMPtr<nsIXFormsRepeatElement> repeatEl(do_QueryInterface(repeatNode));
    NS_ENSURE_STATE(repeatEl);

    rv = repeatEl->HandleNodeInsert(aNode);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}
 

NS_HIDDEN_(nsresult)
NS_NewXFormsInsertElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsInsertDeleteElement(PR_TRUE);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}


NS_HIDDEN_(nsresult)
NS_NewXFormsDeleteElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsInsertDeleteElement(PR_FALSE);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}
