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

#include "nsXFormsStubElement.h"
#include "nsIXFormsControl.h"
#include "nsIXTFXMLVisualWrapper.h"
#include "nsIDOMFocusListener.h"
#include "nsVoidArray.h"
#include "nsIDOMElement.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNodeList.h"
#include "nsXFormsUtils.h"
#include "nsXFormsAtoms.h"
#include "nsIModelElementPrivate.h"
#include "nsIXFormsSelectChild.h"
#include "nsArray.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMEventTarget.h"

class nsXFormsSelectElement : public nsXFormsXMLVisualStub,
                              public nsIXFormsControl,
                              public nsIDOMEventListener
{
public:
  nsXFormsSelectElement() : mElement(nsnull) {}

  NS_DECL_ISUPPORTS_INHERITED

  // nsIXTFXMLVisual overrides
  NS_IMETHOD OnCreated(nsIXTFXMLVisualWrapper *aWrapper);

  // nsIXTFVisual overrides
  NS_IMETHOD GetVisualContent(nsIDOMElement **aElement);
  NS_IMETHOD GetInsertionPoint(nsIDOMElement **aPoint);

  // nsIXTFElement overrides
  NS_IMETHOD OnDestroyed();
  NS_IMETHOD ParentChanged(nsIDOMElement *aNewParent);
  NS_IMETHOD WillSetAttribute(nsIAtom *aName, const nsAString &aValue);
  NS_IMETHOD AttributeSet(nsIAtom *aName, const nsAString &aValue);
  NS_IMETHOD ChildInserted(nsIDOMNode *aChild, PRUint32 aIndex);
  NS_IMETHOD ChildAppended(nsIDOMNode *aChild);
  NS_IMETHOD WillRemoveChild(PRUint32 aIndex);

  NS_IMETHOD BeginAddingChildren();
  NS_IMETHOD DoneAddingChildren();

  // nsIXFormsControl
  NS_DECL_NSIXFORMSCONTROL

  // nsIDOMEventListener
  NS_DECL_NSIDOMEVENTLISTENER

private:
  NS_HIDDEN_(void) SelectItemsInList(const nsString &aValueList);
  NS_HIDDEN_(void) SelectCopiedItem(nsIDOMNode *aNode);
  NS_HIDDEN_(void) SelectItemsByValue(nsIDOMNode *aNode,
                                      const nsStringArray &aItems);

  nsCOMPtr<nsIDOMElement> mLabel;
  nsCOMPtr<nsIDOMHTMLSelectElement> mSelect;
  nsIDOMElement *mElement;
  nsVoidArray mOptions;
};

NS_IMPL_ISUPPORTS_INHERITED2(nsXFormsSelectElement,
                             nsXFormsXMLVisualStub,
                             nsIXFormsControl,
                             nsIDOMEventListener)

// nsIXTFXMLVisual

NS_IMETHODIMP
nsXFormsSelectElement::OnCreated(nsIXTFXMLVisualWrapper *aWrapper)
{
  aWrapper->SetNotificationMask(nsIXTFElement::NOTIFY_PARENT_CHANGED |
                                nsIXTFElement::NOTIFY_WILL_SET_ATTRIBUTE |
                                nsIXTFElement::NOTIFY_ATTRIBUTE_SET |
                                nsIXTFElement::NOTIFY_CHILD_INSERTED |
                                nsIXTFElement::NOTIFY_CHILD_REMOVED |
                                nsIXTFElement::NOTIFY_WILL_REMOVE_CHILD |
                                nsIXTFElement::NOTIFY_BEGIN_ADDING_CHILDREN |
                                nsIXTFElement::NOTIFY_DONE_ADDING_CHILDREN);

  nsCOMPtr<nsIDOMElement> node;
  aWrapper->GetElementNode(getter_AddRefs(node));

  // It's ok to keep pointer to mElement.  mElement will have an
  // owning reference to this object, so as long as we null out mElement in
  // OnDestroyed, it will always be valid.

  mElement = node;
  NS_ASSERTION(mElement, "Wrapper is not an nsIDOMElement, we'll crash soon");

  // Our anonymous content structure will look like this:
  //
  // <label>              (mLabel)
  //   <span/>            (insertion point)
  //   <select/>           (mSelect)
  // </label>
  //

  nsCOMPtr<nsIDOMDocument> domDoc;
  node->GetOwnerDocument(getter_AddRefs(domDoc));

  domDoc->CreateElementNS(NS_LITERAL_STRING(NS_NAMESPACE_XHTML),
                          NS_LITERAL_STRING("label"),
                          getter_AddRefs(mLabel));
  NS_ENSURE_TRUE(mLabel, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMElement> element;
  nsCOMPtr<nsIDOMNode> childReturn;
  domDoc->CreateElementNS(NS_LITERAL_STRING(NS_NAMESPACE_XHTML),
                          NS_LITERAL_STRING("span"),
                          getter_AddRefs(element));
  NS_ENSURE_TRUE(element, NS_ERROR_FAILURE);

  mLabel->AppendChild(element, getter_AddRefs(childReturn));

  domDoc->CreateElementNS(NS_LITERAL_STRING(NS_NAMESPACE_XHTML),
                          NS_LITERAL_STRING("select"),
                          getter_AddRefs(element));

  mSelect = do_QueryInterface(element);
  NS_ENSURE_TRUE(mSelect, NS_ERROR_FAILURE);

  // Unless we're a select1, set multiple=true
  nsAutoString tag;
  mElement->GetLocalName(tag);
  if (!tag.EqualsLiteral("select1")) {
    mSelect->SetMultiple(PR_TRUE);

    // TODO: should leave default size=1 for select1, but options don't
    // select correctly if we do that.  Need to figure out why.
    //    mSelect->SetSize(4);
  }

  // Default to minimal/compact appearance
  mSelect->SetSize(4);

  // TODO: support "selection" and "incremental" attributes.

  mLabel->AppendChild(mSelect, getter_AddRefs(childReturn));

  nsCOMPtr<nsIDOMEventTarget> targ = do_QueryInterface(mSelect);
  NS_ASSERTION(targ, "Select must be an event target");

  // We need an onchange handler to support non-incremental mode, and
  // a blur handler to support incremental mode.
  targ->AddEventListener(NS_LITERAL_STRING("change"), this, PR_FALSE);
  targ->AddEventListener(NS_LITERAL_STRING("blur"), this, PR_FALSE);
  
  return NS_OK;
}

// nsIXTFVisual

NS_IMETHODIMP
nsXFormsSelectElement::GetVisualContent(nsIDOMElement **aElement)
{
  NS_ADDREF(*aElement = mLabel);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSelectElement::GetInsertionPoint(nsIDOMElement **aPoint)
{
  nsCOMPtr<nsIDOMNode> childNode;
  mLabel->GetFirstChild(getter_AddRefs(childNode));
  return CallQueryInterface(childNode, aPoint);
}

// nsIXTFElement

NS_IMETHODIMP
nsXFormsSelectElement::OnDestroyed()
{
  mElement = nsnull;

  nsCOMPtr<nsIDOMEventTarget> targ = do_QueryInterface(mSelect);
  if (NS_LIKELY(targ != nsnull)) {
    targ->RemoveEventListener(NS_LITERAL_STRING("blur"), this, PR_FALSE);
    targ->RemoveEventListener(NS_LITERAL_STRING("change"), this, PR_FALSE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSelectElement::ParentChanged(nsIDOMElement *aNewParent)
{
  // We need to re-evaluate our instance data binding when our parent
  // changes, since xmlns declarations in effect could have changed.
  if (aNewParent)
    Refresh();

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSelectElement::WillSetAttribute(nsIAtom *aName,
                                        const nsAString &aValue)
{
  if (aName == nsXFormsAtoms::bind || aName == nsXFormsAtoms::ref) {
    nsCOMPtr<nsIDOMNode> model;
    nsCOMPtr<nsIDOMElement> bind, context;

    nsXFormsUtils::GetNodeContext(mElement,
                                  nsXFormsUtils::ELEMENT_WITH_MODEL_ATTR,
                                  getter_AddRefs(model),
                                  getter_AddRefs(bind),
                                  getter_AddRefs(context));

    if (model) {
      nsCOMPtr<nsIModelElementPrivate> modelPrivate = do_QueryInterface(model);
      modelPrivate->RemoveFormControl(this);
    }
  } else if (aName == nsXFormsAtoms::appearance) {
    //    if (aValue.EqualsLiteral("full")) {
      // XXX todo
    //    }

    mSelect->SetSize(4);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSelectElement::AttributeSet(nsIAtom *aName, const nsAString &aValue)
{
  if (aName == nsXFormsAtoms::bind || aName == nsXFormsAtoms::ref) {
    Refresh();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSelectElement::ChildInserted(nsIDOMNode *aChild, PRUint32 aIndex)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSelectElement::ChildAppended(nsIDOMNode *aChild)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSelectElement::WillRemoveChild(PRUint32 aIndex)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSelectElement::BeginAddingChildren()
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsSelectElement::DoneAddingChildren()
{
  Refresh();
  return NS_OK;
}

// nsIXFormsControl

NS_IMETHODIMP
nsXFormsSelectElement::Refresh()
{
  nsCOMPtr<nsIDOMNode> childNode, nodeReturn;
  while (NS_SUCCEEDED(mSelect->GetFirstChild(getter_AddRefs(childNode))) &&
         childNode) {

    mSelect->RemoveChild(childNode, getter_AddRefs(nodeReturn));
  }

  nsCOMPtr<nsIDOMNodeList> childNodes;
  nsresult rv = mElement->GetChildNodes(getter_AddRefs(childNodes));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIXFormsSelectChild> childItem;
  nsCOMPtr<nsIArray> childContent;

  PRUint32 childCount;
  childNodes->GetLength(&childCount);

  for (PRUint32 i = 0; i < childCount; ++i) {
    childNodes->Item(i, getter_AddRefs(childNode));
    childItem = do_QueryInterface(childNode);
    if (!childItem)
      continue;

    childItem->GetAnonymousNodes(getter_AddRefs(childContent));

    PRUint32 nodeCount;
    childContent->GetLength(&nodeCount);

    for (PRUint32 j = 0; j < nodeCount; ++j) {
      childNode = do_QueryElementAt(childContent, j);
      mSelect->AppendChild(childNode, getter_AddRefs(nodeReturn));
    }
    // Deselect all of the options.  We'll resync with our bound node below.
    mSelect->SetSelectedIndex(-1);
  }

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

    nsCOMPtr<nsIDOMNode> resultNode;
    if (result)
      result->GetSingleNodeValue(getter_AddRefs(resultNode));

    if (!resultNode)
      return NS_OK;

    nsCOMPtr<nsIDOMNodeList> children;
    resultNode->GetChildNodes(getter_AddRefs(children));

    if (!children)
      return NS_OK;

    nsCOMPtr<nsIDOMNode> child;
    PRUint32 childCount;
    children->GetLength(&childCount);

    for (PRUint32 i = 0; i < childCount; ++i) {
      children->Item(i, getter_AddRefs(child));

      PRUint16 nodeType;
      child->GetNodeType(&nodeType);
      switch (nodeType) {
      case nsIDOMNode::TEXT_NODE:
        {
          nsAutoString nodeValue;
          child->GetNodeValue(nodeValue);
          SelectItemsInList(nodeValue);
        }
        break;

      case nsIDOMNode::ELEMENT_NODE:
        SelectCopiedItem(child);
        break;

      default:
        break;
      }
    }
  }

  return NS_OK;
}

// nsIDOMEventListener

NS_IMETHODIMP
nsXFormsSelectElement::HandleEvent(nsIDOMEvent *aEvent)
{
  nsAutoString type;
  aEvent->GetType(type);

  nsAutoString value;
  mElement->GetAttribute(NS_LITERAL_STRING("incremental"), value);
  PRBool isIncremental = value.EqualsLiteral("true");

  if ((isIncremental && type.EqualsLiteral("change")) ||
      (!isIncremental && type.EqualsLiteral("blur"))) {

    // Update the instance data with our selected items.
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


    nsCOMPtr<nsIDOMNode> resultNode;
    if (result)
      result->GetSingleNodeValue(getter_AddRefs(resultNode));

    if (!resultNode)
      return NS_OK;

    nsCOMPtr<nsIDOMNodeList> children;
    nsresult rv = mElement->GetChildNodes(getter_AddRefs(children));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMNode> child;
    nsCOMPtr<nsIXFormsSelectChild> childItem;
    PRUint32 childCount;
    children->GetLength(&childCount);

    for (PRUint32 i = 0; i < childCount; ++i) {
      children->Item(i, getter_AddRefs(child));
      childItem = do_QueryInterface(child);
      if (childItem) {
        childItem->WriteSelectedItems(resultNode);
      }
    }
  }

  return NS_OK;
}

// internal methods

inline PRBool
IsWhiteSpace(PRUnichar c)
{
  return (c == PRUnichar(' ') || c == PRUnichar('\r') || c == PRUnichar('\n')
          || c == PRUnichar('\t'));
}

void
nsXFormsSelectElement::SelectItemsInList(const nsString &aValueList)
{
  // Start by separating out the space-separated list of values.

  const PRUnichar *start = aValueList.get();
  nsStringArray valueList;

  do {
    // Skip past any whitespace.
    while (*start && IsWhiteSpace(*start))
      ++start;

    const PRUnichar *valueStart = start;

    // Now search for the end of the value
    while (*start && !IsWhiteSpace(*start))
      ++start;

    valueList.AppendString(nsDependentSubstring(valueStart, start));
  } while (*start);

  // Now walk our children and figure out which ones to select.
  // We use a helper function for this that can recurse into choices
  // and itemset nodes.
  SelectItemsByValue(mElement, valueList);
}

void
nsXFormsSelectElement::SelectItemsByValue(nsIDOMNode          *aNode,
                                          const nsStringArray &aItems)
{
  // If we have no children, we can stop immediately.
  nsCOMPtr<nsIDOMNodeList> children;
  aNode->GetChildNodes(getter_AddRefs(children));
  if (!children)
    return;

  PRUint32 childCount;
  children->GetLength(&childCount);

  nsAutoString temp;
  nsCOMPtr<nsIDOMNode> child;
  nsCOMPtr<nsIXFormsSelectChild> item;

  for (PRUint32 i = 0; i < childCount; ++i) {
    children->Item(i, getter_AddRefs(child));
    item = do_QueryInterface(child);

    if (item)
      item->SelectItemsByValue(aItems);
  }
}

void
nsXFormsSelectElement::SelectCopiedItem(nsIDOMNode *aNode)
{
  // TODO: this should select the item which is a copy of |aNode|.
}

NS_HIDDEN_(nsresult)
NS_NewXFormsSelectElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsSelectElement();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}
