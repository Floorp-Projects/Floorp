/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsChildContentList_h__
#define nsChildContentList_h__

#include "nsISupportsImpl.h"
#include "nsINodeList.h"            // base class
#include "js/TypeDecls.h"     // for Handle, Value, JSObject, JSContext

class nsIContent;
class nsINode;

/**
 * Class that implements the nsIDOMNodeList interface (a list of children of
 * the content), by holding a reference to the content and delegating GetLength
 * and Item to its existing child list.
 * @see nsIDOMNodeList
 */
class nsChildContentList final : public nsINodeList
{
public:
  explicit nsChildContentList(nsINode* aNode)
    : mNode(aNode)
  {
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS(nsChildContentList)

  // nsWrapperCache
  virtual JSObject* WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override;

  // nsIDOMNodeList interface
  NS_DECL_NSIDOMNODELIST

  // nsINodeList interface
  virtual int32_t IndexOf(nsIContent* aContent) override;
  virtual nsIContent* Item(uint32_t aIndex) override;

  void DropReference()
  {
    mNode = nullptr;
  }

  virtual nsINode* GetParentObject() override
  {
    return mNode;
  }

private:
  ~nsChildContentList() {}

  // The node whose children make up the list.
  // This is a non-owning ref which is safe because it's set to nullptr by
  // DropReference() by the node slots get destroyed.
  nsINode* MOZ_NON_OWNING_REF mNode;
};

#endif /* nsChildContentList_h__ */
