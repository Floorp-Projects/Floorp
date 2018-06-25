/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ChromeNodeList.h"
#include "mozilla/dom/ChromeNodeListBinding.h"

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<ChromeNodeList>
ChromeNodeList::Constructor(const GlobalObject& aGlobal, ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindowInner> win = do_QueryInterface(aGlobal.GetAsSupports());
  nsIDocument* root = win ? win->GetExtantDoc() : nullptr;
  RefPtr<ChromeNodeList> list = new ChromeNodeList(root);
  return list.forget();
}

JSObject*
ChromeNodeList::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ChromeNodeList_Binding::Wrap(aCx, this, aGivenProto);
}

void
ChromeNodeList::Append(nsINode& aNode, ErrorResult& aError)
{
  if (!aNode.IsContent()) {
    // nsINodeList deals with nsIContent objects only, so need to
    // filter out other nodes for now.
    aError.Throw(NS_ERROR_DOM_TYPE_ERR);
    return;
  }

  AppendElement(aNode.AsContent());
}

void
ChromeNodeList::Remove(nsINode& aNode, ErrorResult& aError)
{
  if (!aNode.IsContent()) {
    aError.Throw(NS_ERROR_DOM_TYPE_ERR);
    return;
  }

  RemoveElement(aNode.AsContent());
}
