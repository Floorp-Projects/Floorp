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
 *  Aaron Reed <aaronr@us.ibm.com> (original author)
 *  Alexander Surkov <surkov.alexander@gmail.com>
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

#include "nsXFormsUtilityService.h"

#include "nsIContent.h"

#include "nsIXFormsControl.h"
#include "nsIXFormsDelegate.h"
#include "nsIXFormsAccessors.h"
#include "nsIXFormsRangeConditionAccessors.h"
#include "nsIXFormsRangeAccessors.h"
#include "nsIXFormsUIWidget.h"
#include "nsIXFormsItemSetUIElement.h"
#include "nsIXFormsComboboxUIWidget.h"
#include "nsIXFormsNSEditableElement.h"
#include "nsIXFormsNSSelectElement.h"
#include "nsIXFormsNSSelect1Element.h"
#include "nsIXFormsItemElement.h"
#include "nsIModelElementPrivate.h"
#include "nsXFormsUtils.h"
#include "nsXFormsAtoms.h"

NS_IMPL_ISUPPORTS1(nsXFormsUtilityService, nsIXFormsUtilityService)

#define GET_XFORMS_ACCESSORS \
NS_ENSURE_ARG(aElement);\
nsCOMPtr<nsIXFormsDelegate> delegate(do_QueryInterface(aElement));\
NS_ENSURE_TRUE(delegate, NS_ERROR_FAILURE);\
nsCOMPtr<nsIXFormsAccessors> accessors;\
delegate->GetXFormsAccessors(getter_AddRefs(accessors));\
NS_ENSURE_TRUE(accessors, NS_ERROR_FAILURE);

#define GET_XFORMS_UIWIDGET \
NS_ENSURE_ARG(aElement);\
nsCOMPtr<nsIXFormsUIWidget> widget(do_QueryInterface(aElement));\
NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);\

#define GET_COMBOBOX_UIWIDGET \
NS_ENSURE_ARG(aElement);\
nsCOMPtr<nsIXFormsComboboxUIWidget> widget(do_QueryInterface(aElement));\
if (!widget) {\
  nsCOMPtr<nsIContent> content(do_QueryInterface(aElement));\
  widget = do_QueryInterface(content->GetBindingParent());\
}\
NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);\

#define GET_XFORMS_SELECT1 \
NS_ENSURE_ARG(aElement);\
nsCOMPtr<nsIXFormsNSSelect1Element> widget(do_QueryInterface(aElement));\
NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);\

#define GET_XFORMS_SELECT \
NS_ENSURE_ARG(aElement);\
nsCOMPtr<nsIXFormsNSSelectElement> widget(do_QueryInterface(aElement));\
NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);\

NS_IMETHODIMP
nsXFormsUtilityService::GetBuiltinTypeName(nsIDOMNode *aElement,
                                           nsAString& aName)
{
  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(aElement));
  NS_ENSURE_TRUE(element, NS_ERROR_FAILURE);

  nsCOMPtr<nsIModelElementPrivate> model = nsXFormsUtils::GetModel(element);
  NS_ENSURE_TRUE(model, NS_ERROR_FAILURE);

  nsCOMPtr<nsIXFormsControl> control(do_QueryInterface(element));
  return model->GetBuiltinTypeNameForControl(control, aName);
}

NS_IMETHODIMP
nsXFormsUtilityService::IsReadonly(nsIDOMNode *aElement, PRBool *aState)
{
  NS_ENSURE_ARG_POINTER(aState);

  GET_XFORMS_ACCESSORS
  return accessors->IsReadonly(aState);
}

NS_IMETHODIMP
nsXFormsUtilityService::IsRelevant(nsIDOMNode *aElement, PRBool *aState)
{
  NS_ENSURE_ARG_POINTER(aState);

  GET_XFORMS_ACCESSORS
  return accessors->IsRelevant(aState);
}

NS_IMETHODIMP
nsXFormsUtilityService::IsRequired(nsIDOMNode *aElement, PRBool *aState)
{
  NS_ENSURE_ARG_POINTER(aState);

  GET_XFORMS_ACCESSORS
  return accessors->IsRequired(aState);
}

NS_IMETHODIMP
nsXFormsUtilityService::IsValid(nsIDOMNode *aElement, PRBool *aState)
{
  NS_ENSURE_ARG_POINTER(aState);

  GET_XFORMS_ACCESSORS
  return accessors->IsValid(aState);
}

NS_IMETHODIMP
nsXFormsUtilityService::IsInRange(nsIDOMNode *aElement, PRUint32 *aState)
{
  NS_ENSURE_ARG_POINTER(aState);
  *aState = STATE_NOT_A_RANGE;

  GET_XFORMS_ACCESSORS
  nsCOMPtr<nsIXFormsRangeConditionAccessors> raccessors(
    do_QueryInterface(accessors));
  if (!raccessors)
    return NS_OK;

  PRBool isInRange = PR_FALSE;
  nsresult rv = raccessors->IsInRange(&isInRange);
  NS_ENSURE_SUCCESS(rv, rv);

  if (isInRange)
    *aState = STATE_IN_RANGE;
  else
    *aState = STATE_OUT_OF_RANGE;

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsUtilityService::GetValue(nsIDOMNode *aElement, nsAString& aValue)
{
  NS_ENSURE_ARG(aElement);

  nsCOMPtr<nsIXFormsUIWidget> widget(do_QueryInterface(aElement));
  if (widget)
    return widget->GetCurrentValue(aValue);

  nsCOMPtr<nsIXFormsItemElement> item(do_QueryInterface(aElement));
  NS_ENSURE_TRUE(aElement, NS_ERROR_FAILURE);

  return item->GetValue(aValue);
}

NS_IMETHODIMP
nsXFormsUtilityService::Focus(nsIDOMNode *aElement)
{
  GET_XFORMS_UIWIDGET
  PRBool advanced = PR_FALSE;
  return widget->Focus(&advanced);
}

NS_IMETHODIMP
nsXFormsUtilityService::GetRangeStart(nsIDOMNode *aElement, nsAString& aValue)
{
  GET_XFORMS_ACCESSORS

  nsCOMPtr<nsIXFormsRangeAccessors> raccessors(do_QueryInterface(accessors));
  NS_ENSURE_TRUE(raccessors, NS_ERROR_FAILURE);
  return raccessors->GetRangeStart(aValue);
}

NS_IMETHODIMP
nsXFormsUtilityService::GetRangeEnd(nsIDOMNode *aElement, nsAString& aValue)
{
  GET_XFORMS_ACCESSORS

  nsCOMPtr<nsIXFormsRangeAccessors> raccessors(do_QueryInterface(accessors));
  NS_ENSURE_TRUE(raccessors, NS_ERROR_FAILURE);
  return raccessors->GetRangeEnd(aValue);
}

NS_IMETHODIMP
nsXFormsUtilityService::GetRangeStep(nsIDOMNode *aElement, nsAString& aValue)
{
  GET_XFORMS_ACCESSORS

  nsCOMPtr<nsIXFormsRangeAccessors> raccessors(do_QueryInterface(accessors));
  NS_ENSURE_TRUE(raccessors, NS_ERROR_FAILURE);
  return raccessors->GetRangeStep(aValue);
}

NS_IMETHODIMP
nsXFormsUtilityService::GetEditor(nsIDOMNode *aElement, nsIEditor **aEditor)
{
  NS_ENSURE_ARG(aElement);
  NS_ENSURE_ARG_POINTER(aEditor);

  nsCOMPtr<nsIXFormsNSEditableElement> editable(do_QueryInterface(aElement));
  NS_ENSURE_TRUE(editable, NS_ERROR_FAILURE);

  return editable->GetEditor(aEditor);
}

NS_IMETHODIMP
nsXFormsUtilityService::IsDropmarkerOpen(nsIDOMNode *aElement, PRBool *aIsOpen)
{
  NS_ENSURE_ARG_POINTER(aIsOpen);

  GET_COMBOBOX_UIWIDGET
  return widget->GetOpen(aIsOpen);
}

NS_IMETHODIMP
nsXFormsUtilityService::ToggleDropmarkerState(nsIDOMNode *aElement)
{
  GET_COMBOBOX_UIWIDGET

  PRBool isOpen = PR_FALSE;
  nsresult rv = widget->GetOpen(&isOpen);
  NS_ENSURE_SUCCESS(rv, rv);
  return widget->SetOpen(!isOpen);
}

// xforms:select1 utility methods

NS_IMETHODIMP
nsXFormsUtilityService::GetSelectedItemForSelect1(nsIDOMNode *aElement,
                                                  nsIDOMNode **aItem)
{
  NS_ENSURE_ARG_POINTER(aItem);

  GET_XFORMS_SELECT1
  return widget->GetSelectedItem(aItem);
}

NS_IMETHODIMP
nsXFormsUtilityService::SetSelectedItemForSelect1(nsIDOMNode *aElement,
                                                  nsIDOMNode *aItem)
{
  NS_ENSURE_ARG(aItem);

  GET_XFORMS_SELECT1
  return widget->SetSelectedItem(aItem);
}

// xforms:select unility methods

NS_IMETHODIMP
nsXFormsUtilityService::GetSelectedItemsForSelect(nsIDOMNode *aElement,
                                                  nsIDOMNodeList **aItems)
{
  NS_ENSURE_ARG_POINTER(aItems);

  GET_XFORMS_SELECT
  return widget->GetSelectedItems(aItems);
}

NS_IMETHODIMP
nsXFormsUtilityService::AddItemToSelectionForSelect(nsIDOMNode *aElement,
                                                    nsIDOMNode *aItem)
{
  NS_ENSURE_ARG(aItem);

  GET_XFORMS_SELECT
  return widget->AddItemToSelection(aItem);
}

NS_IMETHODIMP
nsXFormsUtilityService::RemoveItemFromSelectionForSelect(nsIDOMNode *aElement,
                                                         nsIDOMNode *aItem)
{
  NS_ENSURE_ARG(aItem);

  GET_XFORMS_SELECT
  return widget->RemoveItemFromSelection(aItem);
}

NS_IMETHODIMP
nsXFormsUtilityService::ClearSelectionForSelect(nsIDOMNode *aElement)
{
  GET_XFORMS_SELECT
  return widget->ClearSelection();
}

NS_IMETHODIMP
nsXFormsUtilityService::SelectAllItemsForSelect(nsIDOMNode *aElement)
{
  GET_XFORMS_SELECT
  return widget->SelectAll();
}

NS_IMETHODIMP
nsXFormsUtilityService::IsSelectItemSelected(nsIDOMNode *aElement,
                                             nsIDOMNode *aItem,
                                             PRBool *aIsSelected)
{
  NS_ENSURE_ARG(aItem);
  NS_ENSURE_ARG_POINTER(aIsSelected);

  GET_XFORMS_SELECT
  return widget->IsItemSelected(aItem, aIsSelected);
}

NS_IMETHODIMP
nsXFormsUtilityService::GetSelectChildrenFor(nsIDOMNode *aElement,
                                             nsIDOMNodeList **aItemsList)
{
  NS_ENSURE_ARG(aElement);
  NS_ENSURE_ARG_POINTER(aItemsList);

  nsNodeList* itemsList = new nsNodeList();
  NS_ENSURE_TRUE(itemsList, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = GetSelectChildrenForNode(aElement, itemsList);
  if (NS_FAILED(rv)) {
    delete itemsList;
    return rv;
  }

  NS_ADDREF(*aItemsList = itemsList);
  return NS_OK;
}

// nsXFormsUtilityService

nsresult
nsXFormsUtilityService::GetSelectChildrenForNode(nsIDOMNode *aElement,
                                                 nsNodeList *aNodeList)
{
  nsCOMPtr<nsIContent> contentElm(do_QueryInterface(aElement));
  NS_ENSURE_TRUE(contentElm, NS_ERROR_FAILURE);

  nsCOMPtr<nsINodeInfo> nodeInfo(contentElm->NodeInfo());
  if (!nodeInfo->NamespaceEquals(NS_LITERAL_STRING(NS_NAMESPACE_XFORMS)))
    return NS_ERROR_FAILURE;

  if (nodeInfo->Equals(nsXFormsAtoms::select) ||
      nodeInfo->Equals(nsXFormsAtoms::select1) ||
      nodeInfo->Equals(nsXFormsAtoms::choices)) {
    return GetSelectChildrenForNodeInternal(aElement, aNodeList);
  }

  if (!nodeInfo->Equals(nsXFormsAtoms::itemset))
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIXFormsItemSetUIElement> itemset(do_QueryInterface(aElement));
  NS_ENSURE_TRUE(itemset, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMElement> itemsetContent;
  itemset->GetAnonymousItemSetContent(getter_AddRefs(itemsetContent));
  NS_ENSURE_TRUE(itemsetContent, NS_ERROR_FAILURE);

  return GetSelectChildrenForNodeInternal(itemsetContent, aNodeList);
}

nsresult
nsXFormsUtilityService::GetSelectChildrenForNodeInternal(nsIDOMNode *aElement,
                                                         nsNodeList *aNodeList)
{
  NS_ENSURE_ARG(aElement);

  nsCOMPtr<nsIDOMNodeList> children;
  aElement->GetChildNodes(getter_AddRefs(children));
  NS_ENSURE_TRUE(children, NS_ERROR_FAILURE);

  PRUint32 length = 0;
  children->GetLength(&length);

  for (PRUint32 index = 0; index < length; index++) {
    nsCOMPtr<nsIDOMNode> child;
    children->Item(index, getter_AddRefs(child));
    NS_ENSURE_TRUE(child, NS_ERROR_FAILURE);

    nsCOMPtr<nsIContent> content(do_QueryInterface(child));
    NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);

    nsCOMPtr<nsINodeInfo> nodeInfo(content->NodeInfo());
    if (nodeInfo->NamespaceEquals(NS_LITERAL_STRING(NS_NAMESPACE_XFORMS))) {
      if (nodeInfo->Equals(nsXFormsAtoms::item) ||
          nodeInfo->Equals(nsXFormsAtoms::choices)) {
        aNodeList->AppendElement(content);
      } else if (nodeInfo->Equals(nsXFormsAtoms::itemset)) {
        nsresult rv = GetSelectChildrenForNode(child, aNodeList);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }

  return NS_OK;
}

// nsNodeList

NS_IMPL_ISUPPORTS1(nsNodeList, nsIDOMNodeList)

nsNodeList::nsNodeList()
{
}

nsNodeList::~nsNodeList()
{
}

NS_IMETHODIMP
nsNodeList::GetLength(PRUint32* aLength)
{
  NS_ENSURE_ARG_POINTER(aLength);

  *aLength = mElements.Count();
  return NS_OK;
}

NS_IMETHODIMP
nsNodeList::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  nsISupports *tmp = mElements.SafeObjectAt(aIndex);

  if (!tmp) {
    *aReturn = nsnull;
    return NS_OK;
  }

  return CallQueryInterface(tmp, aReturn);
}

void
nsNodeList::AppendElement(nsIContent *aContent)
{
  mElements.AppendObject(aContent);
}

