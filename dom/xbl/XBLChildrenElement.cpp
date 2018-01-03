/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/XBLChildrenElement.h"
#include "nsCharSeparatedTokenizer.h"
#include "mozilla/dom/NodeListBinding.h"
#include "nsAttrValueOrString.h"

namespace mozilla {
namespace dom {

XBLChildrenElement::~XBLChildrenElement()
{
}

NS_IMPL_ISUPPORTS_INHERITED(XBLChildrenElement,
                            Element,
                            nsIDOMNode,
                            nsIDOMElement)

NS_IMPL_ELEMENT_CLONE(XBLChildrenElement)

nsresult
XBLChildrenElement::BeforeSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                  const nsAttrValueOrString* aValue,
                                  bool aNotify)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::includes) {
      mIncludes.Clear();
      if (aValue) {
        nsCharSeparatedTokenizer tok(aValue->String(), '|',
                                     nsCharSeparatedTokenizer::SEPARATOR_OPTIONAL);
        while (tok.hasMoreTokens()) {
          mIncludes.AppendElement(NS_Atomize(tok.nextToken()));
        }
      }
    }
  }

  return nsXMLElement::BeforeSetAttr(aNamespaceID, aName, aValue, aNotify);
}

} // namespace dom
} // namespace mozilla

using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(nsAnonymousContentList, mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsAnonymousContentList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsAnonymousContentList)

NS_INTERFACE_TABLE_HEAD(nsAnonymousContentList)
  NS_WRAPPERCACHE_INTERFACE_TABLE_ENTRY
  NS_INTERFACE_TABLE(nsAnonymousContentList, nsINodeList, nsIDOMNodeList)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(nsAnonymousContentList)
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
      if (point->HasInsertedChildren()) {
        count += point->InsertedChildrenLength();
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
      if (point->HasInsertedChildren()) {
        if (remIndex < point->InsertedChildrenLength()) {
          return point->InsertedChild(remIndex);
        }
        remIndex -= point->InsertedChildrenLength();
      }
      else {
        if (remIndex < point->GetChildCount()) {
          return point->GetChildAt_Deprecated(remIndex);
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

  int32_t index = 0;
  for (nsIContent* child = mParent->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    if (child->NodeInfo()->Equals(nsGkAtoms::children, kNameSpaceID_XBL)) {
      XBLChildrenElement* point = static_cast<XBLChildrenElement*>(child);
      if (point->HasInsertedChildren()) {
        int32_t insIndex = point->IndexOfInsertedChild(aContent);
        if (insIndex != -1) {
          return index + insIndex;
        }
        index += point->InsertedChildrenLength();
      }
      else {
        int32_t insIndex = point->IndexOf(aContent);
        if (insIndex != -1) {
          return index + insIndex;
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
nsAnonymousContentList::WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto)
{
  return mozilla::dom::NodeListBinding::Wrap(cx, this, aGivenProto);
}
