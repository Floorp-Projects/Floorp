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
 * The Original Code is Mozilla XForms support.
 *
 * The Initial Developer of the Original Code is
 * Novell, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Allan Beaufour <abeaufour@novell.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsIXTFBindableElementWrapper.h"

#include "nsCOMPtr.h"
#include "nsString.h"

#include "nsIDOM3Node.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMSerializer.h"
#include "nsIDOMXPathResult.h"

#include "nsXFormsControlStub.h"
#include "nsIModelElementPrivate.h"
#include "nsIXFormsContextControl.h"
#include "nsIXFormsRepeatItemElement.h"
#include "nsIXFormsRepeatElement.h"
#include "nsXFormsUtils.h"

#ifdef DEBUG
//#define DEBUG_XF_CONTEXTCONTAINER
#endif

/**
 * Implementation of \<contextcontainer\>.
 * 
 * \<contextcontainer\> is a pseudo-element that is wrapped around each row in
 * an "unrolled" \<repeat\> or \<itemset\>. @see nsXFormsRepeatElement and
 * nsXFormsItemSetElement.
 *
 * @todo Support ::repeat-item and ::repeat-index pseudo-elements. (XXX)
 *       @see http://www.w3.org/TR/xforms/sliceF.html#id2645142
 *       @see http://bugzilla.mozilla.org/show_bug.cgi?id=271724
 */
class nsXFormsContextContainer : public nsXFormsBindableControlStub,
                                 public nsIXFormsRepeatItemElement
{
protected:

  /** The context position for the element */
  PRInt32 mContextPosition;

  /** The context size for the element */
  PRInt32 mContextSize;

  /** Does this element have the repeat-index? */
  PRBool mHasIndex;

public:
  nsXFormsContextContainer()
    : mContextPosition(1), mContextSize(1), mHasIndex(PR_FALSE) {}

  NS_DECL_ISUPPORTS_INHERITED

  // nsIXTFElement overrides
  NS_IMETHOD CloneState(nsIDOMElement *aElement);
  NS_IMETHOD HandleDefault(nsIDOMEvent *aEvent, PRBool *aHandled);

  // nsIXFormsControl
  NS_IMETHOD Bind();
  NS_IMETHOD Refresh();
  NS_IMETHOD SetContext(nsIDOMNode *aContextNode,
                        PRInt32     aContextPosition,
                        PRInt32     aContextSize);
  NS_IMETHOD GetContext(nsAString   &aModelID,
                        nsIDOMNode **aContextNode,
                        PRInt32     *aContextPosition,
                        PRInt32     *aContextSize);
  NS_IMETHOD IsEventTarget(PRBool *aOK);

  // nsIXFormsRepeatItemElement
  NS_DECL_NSIXFORMSREPEATITEMELEMENT

#ifdef DEBUG_smaug
  virtual const char* Name() {
    if (mElement) {
      nsAutoString localName;
      mElement->GetLocalName(localName);
      return NS_ConvertUTF16toUTF8(localName).get();
    }
    return "contextcontainer(inline?)";
  }
#endif
};

NS_IMPL_ISUPPORTS_INHERITED1(nsXFormsContextContainer,
                             nsXFormsBindableControlStub,
                             nsIXFormsRepeatItemElement)


// nsIXTFElement
NS_IMETHODIMP
nsXFormsContextContainer::HandleDefault(nsIDOMEvent *aEvent,
                                        PRBool      *aHandled)
{
  if (!aEvent || !mElement)
    return NS_OK;

  nsAutoString type;
  aEvent->GetType(type);
  if (!type.EqualsLiteral("focus"))
    return nsXFormsBindableControlStub::HandleDefault(aEvent, aHandled);

  if (!nsXFormsUtils::EventHandlingAllowed(aEvent, mElement))
    return NS_OK;
  /*
   * Either we, or an element we contain, has gotten focus, so we need to set
   * the repeat index. This is done through the \<repeat\> the
   * nsXFormsContextContainer belongs to.
   *
   * Start by finding the \<repeat\> (our grandparent):
   * <pre>
   * <repeat> <-- gParent
   *   <div>
   *     <contextcontainer\> <-- this
   *   </div>
   * </repeat>
   * </pre>
   */
  nsCOMPtr<nsIDOMNode> parent;
  mElement->GetParentNode(getter_AddRefs(parent));
  NS_ASSERTION(parent, "how can we get focus without a parent?");
  
  nsCOMPtr<nsIDOMNode> gParent;
  parent->GetParentNode(getter_AddRefs(gParent));
  nsCOMPtr<nsIXFormsRepeatElement> repeat = do_QueryInterface(gParent);

  if (!repeat)
    // Not a child to a \<repeat\>
    return NS_OK;

  // Tell \<repeat\> about the new index position
  PRUint32 tmp = mContextPosition;
  return repeat->SetIndex(&tmp, PR_FALSE);
}

NS_IMETHODIMP
nsXFormsContextContainer::CloneState(nsIDOMElement *aElement)
{
  nsCOMPtr<nsIXFormsContextControl> other = do_QueryInterface(aElement);
  if (!other) {
    NS_WARNING("CloneState called with a different type source node");
    return NS_ERROR_FAILURE;
  }

  nsAutoString modelID;
  PRInt32 position, size;
  other->GetContext(modelID, getter_AddRefs(mBoundNode), &position, &size);

  return NS_OK;
}

// nsIXFormsContextControl

NS_IMETHODIMP
nsXFormsContextContainer::SetContext(nsIDOMNode *aContextNode,
                                     PRInt32     aContextPosition,
                                     PRInt32     aContextSize)
{
  mBoundNode = aContextNode;
  mContextPosition = aContextPosition;
  mContextSize = aContextSize;

  // Remove from old model (if any)
  if (mModel) {
    mModel->RemoveFormControl(this);
  }

  // Add to new model
  mModel = nsXFormsUtils::GetModel(mElement);
  if (mModel) {
    mModel->AddFormControl(this);
    mModel->SetStates(this, mBoundNode);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsContextContainer::GetContext(nsAString      &aModelID,
                                     nsIDOMNode    **aContextNode,
                                     PRInt32        *aContextPosition,
                                     PRInt32        *aContextSize)
{
  nsresult rv = nsXFormsBindableControlStub::GetContext(aModelID,
                                                        aContextNode,
                                                        aContextPosition,
                                                        aContextSize);
  NS_ENSURE_SUCCESS(rv, rv);

  *aContextPosition = mContextPosition;
  *aContextSize = mContextSize;

  return NS_OK;
}

// nsIXFormsControl

NS_IMETHODIMP
nsXFormsContextContainer::Bind()
{
  return NS_OK;
}

nsresult
nsXFormsContextContainer::Refresh()
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsContextContainer::IsEventTarget(PRBool* aOK)
{
  *aOK = PR_FALSE;
  return NS_OK;
}

// nsIXFormsRepeatItemElement
/**
 * @todo Should set/get pseudo-element, not attribute (XXX)
 * @see http://bugzilla.mozilla.org/show_bug.cgi?id=271724
 */
NS_IMETHODIMP
nsXFormsContextContainer::SetIndexState(PRBool aHasIndex)
{
  if (mElement) {
    mHasIndex = aHasIndex;
    NS_NAMED_LITERAL_STRING(classStr, "class");
    if (aHasIndex) {
      mElement->SetAttribute(classStr,
                             NS_LITERAL_STRING("xf-repeat-item xf-repeat-index"));
    } else {
      mElement->SetAttribute(classStr, NS_LITERAL_STRING("xf-repeat-item"));
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsContextContainer::GetIndexState(PRBool *aHasIndex)
{
  NS_ENSURE_ARG(aHasIndex);
  *aHasIndex = mHasIndex;
  return NS_OK;
}

// Factory
NS_HIDDEN_(nsresult)
NS_NewXFormsContextContainer(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsContextContainer();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}
