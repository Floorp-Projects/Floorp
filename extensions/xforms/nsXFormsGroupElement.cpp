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
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsString.h"

#include "nsIDOM3Node.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMXPathResult.h"
#include "nsIDOMHTMLDivElement.h"

#include "nsIXTFXMLVisual.h"
#include "nsIXTFXMLVisualWrapper.h"

#include "nsIXFormsControl.h"
#include "nsIXFormsContextControl.h"
#include "nsIModelElementPrivate.h"
#include "nsXFormsAtoms.h"
#include "nsXFormsStubElement.h"
#include "nsXFormsUtils.h"

#ifdef DEBUG
// #define DEBUG_XF_GROUP
#endif

/**
 * Implementation of the XForms \<group\> control.
 * 
 * @see http://www.w3.org/TR/xforms/slice9.html#id2631290
 *
 * @todo If a \<label\> is the first element child for \<group\> it is the
 * label for the entire group
 *
 * @todo "Setting the input focus on a group results in the focus being set to
 * the first form control in the navigation order within that group."
 * (spec. 9.1.1)
 *
 * @bug If a group only has a model attribute, the group fails to set this for
 * children, as it is impossible to distinguish between a failure and absence
 * of binding attributes when calling EvaluateNodeBinding().
 */
class nsXFormsGroupElement : public nsIXFormsControl,
                             public nsXFormsXMLVisualStub,
                             public nsIXFormsContextControl
{
protected:
  /** The DOM element for the node */
  nsCOMPtr<nsIDOMElement> mElement;

  /** The UI HTML element used to represent the tag */
  nsCOMPtr<nsIDOMHTMLDivElement> mHTMLElement;

  /** Have DoneAddingChildren() been called? */
  PRBool mDoneAddingChildren;

  /** The context node for the children of this element */
  nsCOMPtr<nsIDOMElement> mContextNode;

  /** The current ID of the model node is bound to */
  nsString mModelID;

  /** Process element */
  nsresult Process();
      
public:
  NS_DECL_ISUPPORTS_INHERITED

  // Constructor
  nsXFormsGroupElement();
  ~nsXFormsGroupElement();

  // nsIXTFXMLVisual overrides
  NS_IMETHOD OnCreated(nsIXTFXMLVisualWrapper *aWrapper);
  
  // nsIXTFVisual overrides
  NS_IMETHOD GetVisualContent(nsIDOMElement **aElement);
  NS_IMETHOD GetInsertionPoint(nsIDOMElement **aElement);

  // nsIXTFElement overrides
  NS_IMETHOD OnDestroyed();
  NS_IMETHOD WillSetAttribute(nsIAtom *aName, const nsAString &aValue);
  NS_IMETHOD AttributeSet(nsIAtom *aName, const nsAString &aValue);
  NS_IMETHOD DoneAddingChildren();

  // nsIXFormsControl
  NS_DECL_NSIXFORMSCONTROL

  // nsIXFormsContextControl
  NS_DECL_NSIXFORMSCONTEXTCONTROL
};

NS_IMPL_ISUPPORTS_INHERITED2(nsXFormsGroupElement,
                             nsXFormsXMLVisualStub,
                             nsIXFormsControl,
                             nsIXFormsContextControl)

MOZ_DECL_CTOR_COUNTER(nsXFormsGroupElement)

nsXFormsGroupElement::nsXFormsGroupElement()
  : mElement(nsnull)
{
  MOZ_COUNT_CTOR(nsXFormsGroupElement);
}

nsXFormsGroupElement::~nsXFormsGroupElement()
{
  MOZ_COUNT_DTOR(nsXFormsGroupElement);
}

// nsIXTFXMLVisual
NS_IMETHODIMP
nsXFormsGroupElement::OnCreated(nsIXTFXMLVisualWrapper *aWrapper)
{
#ifdef DEBUG_XF_GROUP
  printf("nsXFormsGroupElement::OnCreated(aWrapper=%p)\n", (void*) aWrapper);
#endif

  // Initialize member(s)
  nsCOMPtr<nsIDOMElement> node;
  aWrapper->GetElementNode(getter_AddRefs(node));
  mElement = node;
  NS_ASSERTION(mElement, "Wrapper is not an nsIDOMElement, we'll crash soon");

  mDoneAddingChildren = PR_FALSE;
  
  // Create HTML tag
  nsCOMPtr<nsIDOMDocument> domDoc;
  node->GetOwnerDocument(getter_AddRefs(domDoc));

  nsCOMPtr<nsIDOMElement> domElement;
  domDoc->CreateElementNS(NS_LITERAL_STRING(NS_NAMESPACE_XHTML),
                          NS_LITERAL_STRING("div"),
                          getter_AddRefs(domElement));

  mHTMLElement = do_QueryInterface(domElement);
  NS_ENSURE_TRUE(mHTMLElement, NS_ERROR_FAILURE);
  
  // Setup which notifications to receive
  aWrapper->SetNotificationMask(nsIXTFElement::NOTIFY_DONE_ADDING_CHILDREN |
                                nsIXTFElement::NOTIFY_ATTRIBUTE_SET |
                                nsIXTFElement::NOTIFY_WILL_SET_ATTRIBUTE);

  return NS_OK;
}

// nsIXTFVisual
NS_IMETHODIMP
nsXFormsGroupElement::GetVisualContent(nsIDOMElement * *aVisualContent)
{
  NS_ADDREF(*aVisualContent = mHTMLElement);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsGroupElement::GetInsertionPoint(nsIDOMElement **aElement)
{
  NS_ADDREF(*aElement = mHTMLElement);
  return NS_OK;
}

// nsIXTFElement
NS_IMETHODIMP
nsXFormsGroupElement::OnDestroyed()
{
  mHTMLElement = nsnull;
  mContextNode = nsnull;
  mElement = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsGroupElement::WillSetAttribute(nsIAtom *aName, const nsAString& aNewValue)
{
  if (aName == nsXFormsAtoms::bind || aName == nsXFormsAtoms::ref) {
    nsCOMPtr<nsIDOMNode> modelNode = nsXFormsUtils::GetModel(mElement);

    nsCOMPtr<nsIModelElementPrivate> model = do_QueryInterface(modelNode);    
    if (model) {
      model->RemoveFormControl(this);
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsGroupElement::AttributeSet(nsIAtom *aName, const nsAString& aNewValue)
{
  if (aName == nsXFormsAtoms::bind || aName == nsXFormsAtoms::ref) {
    Refresh();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsGroupElement::DoneAddingChildren()
{
#ifdef DEBUG_XF_GROUP
  printf("nsXFormsGroupElement::DoneAddingChildren()\n");
#endif

  mDoneAddingChildren = PR_TRUE;
  Refresh();

  return NS_OK;
}

// nsXFormsControl
nsresult
nsXFormsGroupElement::Refresh()
{
#ifdef DEBUG_XF_GROUP
  printf("nsXFormsGroupElement::Refresh(mDoneAddingChildren=%d)\n", mDoneAddingChildren);
#endif
  
  nsresult rv = NS_OK;
  if (mDoneAddingChildren) {
    rv = Process();
  }
  return rv;
}

// nsXFormsGroupElement
nsresult
nsXFormsGroupElement::Process()
{
#ifdef DEBUG_XF_GROUP
  printf("nsXFormsGroupElement::Process()\n");
#endif

  mModelID.Truncate();
  nsCOMPtr<nsIDOMNode> modelNode;
  nsCOMPtr<nsIDOMElement> bindElement;
  nsCOMPtr<nsIDOMXPathResult> result =
    nsXFormsUtils::EvaluateNodeBinding(mElement,
                                       nsXFormsUtils::ELEMENT_WITH_MODEL_ATTR,
                                       NS_LITERAL_STRING("ref"),
                                       EmptyString(),
                                       nsIDOMXPathResult::FIRST_ORDERED_NODE_TYPE,
                                       getter_AddRefs(modelNode),
                                       getter_AddRefs(bindElement));
    
  nsCOMPtr<nsIModelElementPrivate> model = do_QueryInterface(modelNode);
  
  if (model) {
    model->AddFormControl(this);
  }
  
  NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);

  // Get model ID
  nsCOMPtr<nsIDOMElement> modelElement = do_QueryInterface(modelNode);
  NS_ENSURE_TRUE(modelElement, NS_ERROR_FAILURE);
  modelElement->GetAttribute(NS_LITERAL_STRING("id"), mModelID);

  // Get context node, if any  
  nsCOMPtr<nsIDOMNode> singleNode;
  result->GetSingleNodeValue(getter_AddRefs(singleNode));
  mContextNode = do_QueryInterface(singleNode);
  NS_ENSURE_TRUE(mContextNode, NS_ERROR_FAILURE);
  
  return NS_OK;
}

// nsIXFormsContextControl
nsresult
nsXFormsGroupElement::GetContext(nsAString&      aModelID,
                                 nsIDOMElement **aContextNode,
                                 PRInt32        *aContextPosition,
                                 PRInt32        *aContextSize)
{
#ifdef DEBUG_XF_GROUP
  printf("nsXFormsGroupElement::GetContext()\n");
#endif
  NS_ENSURE_ARG(aContextSize);
  NS_ENSURE_ARG(aContextPosition);
  
  /** @todo Not too elegant to call Process() here, but DoneAddingChildren is,
   *        logically, called on children before us.  We need a notification
   *        that goes from the document node and DOWN, where the controls
   *        should Refresh().
   */
  *aContextPosition = 1;
  *aContextSize = 1;

  nsresult rv = Process();
  NS_ENSURE_SUCCESS(rv, rv);

  NS_IF_ADDREF(*aContextNode = mContextNode);
  aModelID = mModelID;
  
  return NS_OK;
}

// Factory
NS_HIDDEN_(nsresult)
NS_NewXFormsGroupElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsGroupElement();
  if (!*aResult)
  return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}
