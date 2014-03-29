/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=79: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/XBLChildrenElement.h"
#include "nsCharSeparatedTokenizer.h"
#include "mozilla/dom/NodeListBinding.h"

namespace mozilla {
namespace dom {

XBLChildrenElement::~XBLChildrenElement()
{
}

NS_IMPL_ADDREF_INHERITED(XBLChildrenElement, Element)
NS_IMPL_RELEASE_INHERITED(XBLChildrenElement, Element)

NS_INTERFACE_TABLE_HEAD(XBLChildrenElement)
  NS_INTERFACE_TABLE_INHERITED2(XBLChildrenElement, nsIDOMNode,
                                                    nsIDOMElement)
  NS_ELEMENT_INTERFACE_TABLE_TO_MAP_SEGUE
NS_INTERFACE_MAP_END_INHERITING(Element)

NS_IMPL_ELEMENT_CLONE(XBLChildrenElement)

nsIAtom*
XBLChildrenElement::GetIDAttributeName() const
{
  return nullptr;
}

nsIAtom*
XBLChildrenElement::DoGetID() const
{
  return nullptr;
}

nsresult
XBLChildrenElement::UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                              bool aNotify)
{
  if (aAttribute == nsGkAtoms::includes &&
      aNameSpaceID == kNameSpaceID_None) {
    mIncludes.Clear();
  }

  return Element::UnsetAttr(aNameSpaceID, aAttribute, aNotify);
}

bool
XBLChildrenElement::ParseAttribute(int32_t aNamespaceID,
                                   nsIAtom* aAttribute,
                                   const nsAString& aValue,
                                   nsAttrValue& aResult)
{
  if (aAttribute == nsGkAtoms::includes &&
      aNamespaceID == kNameSpaceID_None) {
    mIncludes.Clear();
    nsCharSeparatedTokenizer tok(aValue, '|',
                                 nsCharSeparatedTokenizer::SEPARATOR_OPTIONAL);
    while (tok.hasMoreTokens()) {
      nsCOMPtr<nsIAtom> atom = do_GetAtom(tok.nextToken());
      mIncludes.AppendElement(atom);
    }
  }

  return false;
}

} // namespace mozilla
} // namespace dom

using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(nsAnonymousContentList, mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsAnonymousContentList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsAnonymousContentList)

NS_INTERFACE_TABLE_HEAD(nsAnonymousContentList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_TABLE_INHERITED2(nsAnonymousContentList, nsINodeList,
                                                        nsIDOMNodeList)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsAnonymousContentList)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
nsAnonymousContentList::GetLength(uint32_t* aLength)
{
  if (!mParent) {
    *aLength = 0;
    return NS_OK;
  }

  uint32_t count = 0;
  for (nsIContent* child = mParent->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    if (child->NodeInfo()->Equals(nsGkAtoms::children, kNameSpaceID_XBL)) {
      XBLChildrenElement* point = static_cast<XBLChildrenElement*>(child);
      if (!point->mInsertedChildren.IsEmpty()) {
        count += point->mInsertedChildren.Length();
      }
      else {
        count += point->GetChildCount();
      }
    }
    else {
      ++count;
    }
  }

  *aLength = count;

  return NS_OK;
}

NS_IMETHODIMP
nsAnonymousContentList::Item(uint32_t aIndex, nsIDOMNode** aReturn)
{
  nsIContent* item = Item(aIndex);
  if (!item) {
    return NS_ERROR_FAILURE;
  }

  return CallQueryInterface(item, aReturn);
}

nsIContent*
nsAnonymousContentList::Item(uint32_t aIndex)
{
  if (!mParent) {
    return nullptr;
  }

  uint32_t remIndex = aIndex;
  for (nsIContent* child = mParent->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    if (child->NodeInfo()->Equals(nsGkAtoms::children, kNameSpaceID_XBL)) {
      XBLChildrenElement* point = static_cast<XBLChildrenElement*>(child);
      if (!point->mInsertedChildren.IsEmpty()) {
        if (remIndex < point->mInsertedChildren.Length()) {
          return point->mInsertedChildren[remIndex];
        }
        remIndex -= point->mInsertedChildren.Length();
      }
      else {
        if (remIndex < point->GetChildCount()) {
          return point->GetChildAt(remIndex);
        }
        remIndex -= point->GetChildCount();
      }
    }
    else {
      if (remIndex == 0) {
        return child;
      }
      --remIndex;
    }
  }

  return nullptr;
}

int32_t
nsAnonymousContentList::IndexOf(nsIContent* aContent)
{
  NS_ASSERTION(!aContent->NodeInfo()->Equals(nsGkAtoms::children,
                                             kNameSpaceID_XBL),
               "Looking for insertion point");

  if (!mParent) {
    return -1;
  }

  uint32_t index = 0;
  for (nsIContent* child = mParent->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    if (child->NodeInfo()->Equals(nsGkAtoms::children, kNameSpaceID_XBL)) {
      XBLChildrenElement* point = static_cast<XBLChildrenElement*>(child);
      if (!point->mInsertedChildren.IsEmpty()) {
        uint32_t insIndex = point->mInsertedChildren.IndexOf(aContent);
        if (insIndex != point->mInsertedChildren.NoIndex) {
          return index + insIndex;
        }
        index += point->mInsertedChildren.Length();
      }
      else {
        int32_t insIndex = point->IndexOf(aContent);
        if (insIndex != -1) {
          return index + (uint32_t)insIndex;
        }
        index += point->GetChildCount();
      }
    }
    else {
      if (child == aContent) {
        return index;
      }
      ++index;
    }
  }

  return -1;
}

JSObject*
nsAnonymousContentList::WrapObject(JSContext *cx, JS::Handle<JSObject*> scope)
{
  return mozilla::dom::NodeListBinding::Wrap(cx, scope, this);
}
