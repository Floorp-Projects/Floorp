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
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
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

#include "nsIXTFXMLVisual.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOM3Node.h"
#include "nsIDOMElement.h"
#include "nsMemory.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIXTFXMLVisualWrapper.h"
#include "nsIDOMDocument.h"
#include "nsIXFormsControl.h"
#include "nsISchema.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsXFormsAtoms.h"
#include "nsAutoPtr.h"
#include "nsIDOMXPathResult.h"
#include "nsIDOMFocusListener.h"
#include "nsXFormsUtils.h"
#include "nsIModelElementPrivate.h"
#include "nsIContent.h"
#include "nsIDOMXPathExpression.h"

static const nsIID sScriptingIIDs[] = {
  NS_IDOMELEMENT_IID,
  NS_IDOMEVENTTARGET_IID,
  NS_IDOM3NODE_IID
};

class nsXFormsInputElement : public nsIXTFXMLVisual,
                             public nsIDOMFocusListener,
                             public nsIXFormsControl
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIXTFXMLVISUAL
  NS_DECL_NSIXTFVISUAL
  NS_DECL_NSIXTFELEMENT
  NS_DECL_NSIXFORMSCONTROL

  // nsIDOMEventListener
  NS_IMETHOD HandleEvent(nsIDOMEvent *aEvent);

  // nsIDOMFocusListener
  NS_IMETHOD Focus(nsIDOMEvent *aEvent);
  NS_IMETHOD Blur(nsIDOMEvent *aEvent);

  nsXFormsInputElement() : mElement(nsnull) {}

private:
  nsCOMPtr<nsIDOMHTMLInputElement> mInput;
  nsIDOMElement *mElement;
};

NS_IMPL_ADDREF(nsXFormsInputElement)
NS_IMPL_RELEASE(nsXFormsInputElement)

NS_INTERFACE_MAP_BEGIN(nsXFormsInputElement)
  NS_INTERFACE_MAP_ENTRY(nsIXTFXMLVisual)
  NS_INTERFACE_MAP_ENTRY(nsIXTFElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMFocusListener)
  NS_INTERFACE_MAP_ENTRY(nsIXFormsControl)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXTFXMLVisual)
NS_INTERFACE_MAP_END

// nsIXTFXMLVisual

NS_IMETHODIMP
nsXFormsInputElement::OnCreated(nsIXTFXMLVisualWrapper *aWrapper)
{
  aWrapper->SetNotificationMask(nsIXTFElement::NOTIFY_WILL_SET_ATTRIBUTE |
                                nsIXTFElement::NOTIFY_ATTRIBUTE_SET |
                                nsIXTFElement::NOTIFY_PARENT_CHANGED);

  nsCOMPtr<nsIDOMElement> node;
  aWrapper->GetElementNode(getter_AddRefs(node));

  // It's ok to keep a weak pointer to mElement.  mElement will have an
  // owning reference to this object, so as long as we null out mElement in
  // OnDestroyed, it will always be valid.

  mElement = node;
  NS_ASSERTION(mElement, "Wrapper is not an nsIDOMElement, we'll crash soon");

  nsCOMPtr<nsIDOMDocument> domDoc;
  node->GetOwnerDocument(getter_AddRefs(domDoc));

  nsCOMPtr<nsIDOMElement> inputElement;
  domDoc->CreateElementNS(NS_LITERAL_STRING(NS_NAMESPACE_XHTML),
                          NS_LITERAL_STRING("input"),
                          getter_AddRefs(inputElement));

  mInput = do_QueryInterface(inputElement);
  NS_ENSURE_TRUE(mInput, NS_ERROR_FAILURE);

  // We can't use xtf handleDefault here because editor stops blur events from
  // bubbling, and we can't use a system event group handler because blur
  // events don't go to the system event group (bug 263240).  So, just install
  // a bubbling listener in the normal event group.  This works out because
  // content can't get at the anonymous content to install an event handler,
  // and the event doesn't bubble up.

  nsCOMPtr<nsIDOMEventTarget> targ = do_QueryInterface(mInput);
  NS_ASSERTION(targ, "input must be an event target!");

  targ->AddEventListener(NS_LITERAL_STRING("blur"), this, PR_FALSE);
  return NS_OK;
}

// nsIXTFVisual

NS_IMETHODIMP
nsXFormsInputElement::GetVisualContent(nsIDOMElement **aElement)
{
  NS_ADDREF(*aElement = mInput);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInputElement::GetInsertionPoint(nsIDOMElement **aElement)
{
  *aElement = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInputElement::GetApplyDocumentStyleSheets(PRBool *aApply)
{
  *aApply = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInputElement::DidLayout()
{
  return NS_OK;
}

// nsIXTFElement

NS_IMETHODIMP
nsXFormsInputElement::OnDestroyed()
{
  nsCOMPtr<nsIDOMEventTarget> targ = do_QueryInterface(mInput);
  if (NS_LIKELY(targ != nsnull)) {
    targ->RemoveEventListener(NS_LITERAL_STRING("blur"), this, PR_FALSE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInputElement::GetElementType(PRUint32 *aType)
{
  *aType = ELEMENT_TYPE_XML_VISUAL;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInputElement::GetIsAttributeHandler(PRBool *aIsHandler)
{
  *aIsHandler = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInputElement::GetScriptingInterfaces(PRUint32 *aCount, nsIID ***aArray)
{
  return nsXFormsUtils::CloneScriptingInterfaces(sScriptingIIDs,
                                                 NS_ARRAY_LENGTH(sScriptingIIDs),
                                                 aCount, aArray);
}

NS_IMETHODIMP
nsXFormsInputElement::WillChangeDocument(nsIDOMDocument *aNewDocument)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInputElement::DocumentChanged(nsIDOMDocument *aNewDocument)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInputElement::WillChangeParent(nsIDOMElement *aNewParent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInputElement::ParentChanged(nsIDOMElement *aNewParent)
{
  // We need to re-evaluate our instance data binding when our parent
  // changes, since xmlns declarations in effect could have changed.
  Refresh();
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInputElement::WillInsertChild(nsIDOMNode *aChild, PRUint32 aIndex)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInputElement::ChildInserted(nsIDOMNode *aChild, PRUint32 aIndex)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInputElement::WillAppendChild(nsIDOMNode *aChild)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInputElement::ChildAppended(nsIDOMNode *aChild)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInputElement::WillRemoveChild(PRUint32 aIndex)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInputElement::ChildRemoved(PRUint32 aIndex)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInputElement::WillSetAttribute(nsIAtom *aName, const nsAString &aValue)
{
  if (aName == nsXFormsAtoms::bind || aName == nsXFormsAtoms::ref) {
    nsCOMPtr<nsIDOMElement> bindElement;
    nsCOMPtr<nsIModelElementPrivate> model;

    model = do_QueryInterface(nsXFormsUtils::GetModelAndBind(mElement, getter_AddRefs(bindElement)));
    if (model)
      model->RemoveFormControl(this);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInputElement::AttributeSet(nsIAtom *aName, const nsAString &aValue)
{
  if (aName == nsXFormsAtoms::bind || aName == nsXFormsAtoms::ref) {
    Refresh();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInputElement::WillRemoveAttribute(nsIAtom *aName)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInputElement::AttributeRemoved(nsIAtom *aName)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInputElement::DoneAddingChildren()
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInputElement::HandleDefault(nsIDOMEvent *aEvent, PRBool *aHandled)
{
  *aHandled = PR_FALSE;
  return NS_OK;
}

// nsIDOMEventListener

NS_IMETHODIMP
nsXFormsInputElement::HandleEvent(nsIDOMEvent *aEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInputElement::Focus(nsIDOMEvent *aEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInputElement::Blur(nsIDOMEvent *aEvent)
{
  if (!mInput)
    return NS_OK;

  nsCOMPtr<nsIDOMNode> modelNode;
  nsCOMPtr<nsIDOMElement> bindElement;
  nsCOMPtr<nsIDOMXPathResult> result =
    nsXFormsUtils::EvaluateNodeBinding(mElement,
                                       nsIDOMXPathResult::FIRST_ORDERED_NODE_TYPE,
                                       getter_AddRefs(modelNode),
                                       getter_AddRefs(bindElement));

  if (!result)
    return NS_OK;

  nsCOMPtr<nsIDOMNode> singleNode;
  result->GetSingleNodeValue(getter_AddRefs(singleNode));

  if (!singleNode)
    return NS_OK;

  nsAutoString value, type;
  mInput->GetType(type);
  if (type.EqualsLiteral("checkbox")) {
    PRBool checked;
    mInput->GetChecked(&checked);
    value.AssignASCII(checked ? "1" : "0", 1);
  } else {
    mInput->GetValue(value);
  }

  PRUint16 nodeType = 0;
  singleNode->GetNodeType(&nodeType);

  switch (nodeType) {
  case nsIDOMNode::ATTRIBUTE_NODE:
  case nsIDOMNode::TEXT_NODE:
    singleNode->SetNodeValue(value);
    break;
  case nsIDOMNode::ELEMENT_NODE:
    {
      nsCOMPtr<nsIDOM3Node> node = do_QueryInterface(singleNode);
      NS_ASSERTION(node, "DOM Nodes must support DOM3 interfaces");

      node->SetTextContent(value);
      break;
    }
  }

  return NS_OK;
}

// other methods

NS_IMETHODIMP
nsXFormsInputElement::Refresh()
{
  if (!mInput)
    return NS_OK;

  nsCOMPtr<nsIDOMNode> modelNode;
  nsCOMPtr<nsIDOMElement> bindElement;
  nsCOMPtr<nsIDOMXPathResult> result =
    nsXFormsUtils::EvaluateNodeBinding(mElement,
                                       nsIDOMXPathResult::FIRST_ORDERED_NODE_TYPE,
                                       getter_AddRefs(modelNode),
                                       getter_AddRefs(bindElement));

  nsCOMPtr<nsIModelElementPrivate> model = do_QueryInterface(modelNode);

  if (model) {
    model->AddFormControl(this);

    nsCOMPtr<nsIDOMNode> resultNode;
    if (result)
      result->GetSingleNodeValue(getter_AddRefs(resultNode));

    if (resultNode) {
      PRUint16 nodeType = 0;
      resultNode->GetNodeType(&nodeType);

      nsAutoString text;

      switch (nodeType) {
      case nsIDOMNode::TEXT_NODE:
      case nsIDOMNode::ATTRIBUTE_NODE:
        resultNode->GetNodeValue(text);
        break;
      case nsIDOMNode::ELEMENT_NODE:
        {
          nsCOMPtr<nsIDOMNode> firstChild;
          resultNode->GetFirstChild(getter_AddRefs(firstChild));
          if (firstChild)
            firstChild->GetNodeValue(text);
          break;
        }
      default:
        NS_ERROR("form control references invalid node type in instance data");
      }

      nsCOMPtr<nsISchemaType> type;
      model->GetTypeForControl(this, getter_AddRefs(type));
      nsCOMPtr<nsISchemaBuiltinType> biType = do_QueryInterface(type);
      PRUint16 typeValue = nsISchemaBuiltinType::BUILTIN_TYPE_STRING;

      if (biType)
        biType->GetBuiltinType(&typeValue);

      if (typeValue == nsISchemaBuiltinType::BUILTIN_TYPE_BOOLEAN) {
        mInput->SetAttribute(NS_LITERAL_STRING("type"),
                             NS_LITERAL_STRING("checkbox"));

        mInput->SetChecked(text.EqualsLiteral("true") ||
                           text.EqualsLiteral("1"));
      } else {
        mInput->RemoveAttribute(NS_LITERAL_STRING("type"));
        mInput->SetValue(text);
      }

      PRBool isReadOnly = PR_FALSE;
      nsCOMPtr<nsIContent> nodeContent = do_QueryInterface(resultNode);
      if (nodeContent) {
        nsIDOMXPathExpression *expr =
          NS_STATIC_CAST(nsIDOMXPathExpression*,
                         nodeContent->GetProperty(nsXFormsAtoms::readonly));

        if (expr) {
          expr->Evaluate(mElement,
                         nsIDOMXPathResult::BOOLEAN_TYPE, nsnull,
                         getter_AddRefs(result));
          if (result) {
            result->GetBooleanValue(&isReadOnly);
          }
        }
      }

      mInput->SetReadOnly(isReadOnly);
    }
  }

  return NS_OK;
}

NS_HIDDEN_(nsresult)
NS_NewXFormsInputElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsInputElement();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}
