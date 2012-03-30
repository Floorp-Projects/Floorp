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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
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

#include "nsContentCreatorFunctions.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLMenuItemElement.h"
#include "nsXULContextMenuBuilder.h"

using namespace mozilla;
using namespace mozilla::dom;

nsXULContextMenuBuilder::nsXULContextMenuBuilder()
  : mCurrentGeneratedItemId(0)
{
}

nsXULContextMenuBuilder::~nsXULContextMenuBuilder()
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXULContextMenuBuilder)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsXULContextMenuBuilder)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mFragment)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mCurrentNode)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMARRAY(mElements)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsXULContextMenuBuilder)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mFragment)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mCurrentNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMARRAY(mElements)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsXULContextMenuBuilder)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsXULContextMenuBuilder)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsXULContextMenuBuilder)
  NS_INTERFACE_MAP_ENTRY(nsIMenuBuilder)
  NS_INTERFACE_MAP_ENTRY(nsIXULContextMenuBuilder)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIMenuBuilder)
NS_INTERFACE_MAP_END


NS_IMETHODIMP
nsXULContextMenuBuilder::OpenContainer(const nsAString& aLabel)
{
  if (!mFragment) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (!mCurrentNode) {
    mCurrentNode = mFragment;
  } else {
    nsCOMPtr<nsIContent> menu;
    nsresult rv = CreateElement(nsGkAtoms::menu, nsnull, getter_AddRefs(menu));
    NS_ENSURE_SUCCESS(rv, rv);

    menu->SetAttr(kNameSpaceID_None, nsGkAtoms::label, aLabel, false);

    nsCOMPtr<nsIContent> menuPopup;
    rv = CreateElement(nsGkAtoms::menupopup, nsnull,
                       getter_AddRefs(menuPopup));
    NS_ENSURE_SUCCESS(rv, rv);
        
    rv = menu->AppendChildTo(menuPopup, false);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mCurrentNode->AppendChildTo(menu, false);
    NS_ENSURE_SUCCESS(rv, rv);

    mCurrentNode = menuPopup;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXULContextMenuBuilder::AddItemFor(nsIDOMHTMLMenuItemElement* aElement,
                                    bool aCanLoadIcon)
{
  if (!mFragment) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsCOMPtr<nsIContent> menuitem;
  nsresult rv = CreateElement(nsGkAtoms::menuitem, aElement,
                              getter_AddRefs(menuitem));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString type;
  aElement->GetType(type);
  if (type.EqualsLiteral("checkbox") || type.EqualsLiteral("radio")) {
    // The menu is only temporary, so we don't need to handle
    // the radio type precisely.
    menuitem->SetAttr(kNameSpaceID_None, nsGkAtoms::type,
                      NS_LITERAL_STRING("checkbox"), false);
    bool checked;
    aElement->GetChecked(&checked);
    if (checked) {
      menuitem->SetAttr(kNameSpaceID_None, nsGkAtoms::checked,
                        NS_LITERAL_STRING("true"), false);
    }
  }

  nsAutoString label;
  aElement->GetLabel(label);
  menuitem->SetAttr(kNameSpaceID_None, nsGkAtoms::label, label, false);

  nsAutoString icon;
  aElement->GetIcon(icon);
  if (!icon.IsEmpty()) {
    menuitem->SetAttr(kNameSpaceID_None, nsGkAtoms::_class,
                      NS_LITERAL_STRING("menuitem-iconic"), false);
    if (aCanLoadIcon) {
      menuitem->SetAttr(kNameSpaceID_None, nsGkAtoms::image, icon, false);
    }
  }

  bool disabled;
  aElement->GetDisabled(&disabled);
  if (disabled) {
    menuitem->SetAttr(kNameSpaceID_None, nsGkAtoms::disabled,
                      NS_LITERAL_STRING("true"), false);
  }

  return mCurrentNode->AppendChildTo(menuitem, false);
}

NS_IMETHODIMP
nsXULContextMenuBuilder::AddSeparator()
{
  if (!mFragment) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsCOMPtr<nsIContent> menuseparator;
  nsresult rv = CreateElement(nsGkAtoms::menuseparator, nsnull,
                              getter_AddRefs(menuseparator));
  NS_ENSURE_SUCCESS(rv, rv);

  return mCurrentNode->AppendChildTo(menuseparator, false);
}

NS_IMETHODIMP
nsXULContextMenuBuilder::UndoAddSeparator()
{
  if (!mFragment) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  PRUint32 count = mCurrentNode->GetChildCount();
  if (!count ||
      mCurrentNode->GetChildAt(count - 1)->Tag() != nsGkAtoms::menuseparator) {
    return NS_OK;
  }

  mCurrentNode->RemoveChildAt(count - 1, false);
  return NS_OK;
}

NS_IMETHODIMP
nsXULContextMenuBuilder::CloseContainer()
{
  if (!mFragment) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (mCurrentNode == mFragment) {
    mCurrentNode = nsnull;
  } else {
    nsIContent* parent = mCurrentNode->GetParent();
    mCurrentNode = parent->GetParent();
  }

  return NS_OK;
}


NS_IMETHODIMP
nsXULContextMenuBuilder::Init(nsIDOMDocumentFragment* aDocumentFragment,
                              const nsAString& aGeneratedItemIdAttrName)
{
  NS_ENSURE_ARG_POINTER(aDocumentFragment);

  mFragment = do_QueryInterface(aDocumentFragment);
  mDocument = mFragment->GetOwnerDocument();
  mGeneratedItemIdAttr = do_GetAtom(aGeneratedItemIdAttrName);

  return NS_OK;
}

NS_IMETHODIMP
nsXULContextMenuBuilder::Click(const nsAString& aGeneratedItemId)
{
  PRInt32 rv;
  PRInt32 idx = nsString(aGeneratedItemId).ToInteger(&rv);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIDOMHTMLElement> element = mElements.SafeObjectAt(idx);
    if (element) {
      element->Click();
    }
  }

  return NS_OK;
}

nsresult
nsXULContextMenuBuilder::CreateElement(nsIAtom* aTag,
                                       nsIDOMHTMLElement* aHTMLElement,
                                       nsIContent** aResult)
{
  *aResult = nsnull;

  nsCOMPtr<nsINodeInfo> nodeInfo = mDocument->NodeInfoManager()->GetNodeInfo(
    aTag, nsnull, kNameSpaceID_XUL, nsIDOMNode::ELEMENT_NODE);
  NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = NS_NewElement(aResult, nodeInfo.forget(), NOT_FROM_PARSER);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoString generateditemid;

  if (aHTMLElement) {
    mElements.AppendObject(aHTMLElement);
    generateditemid.AppendInt(mCurrentGeneratedItemId++);
  }

  (*aResult)->SetAttr(kNameSpaceID_None, mGeneratedItemIdAttr, generateditemid,
                      false);

  return NS_OK;
}
