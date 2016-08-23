/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef A11Y_AOM_ACCESSIBLENODE_H
#define A11Y_AOM_ACCESSIBLENODE_H

#include "nsWrapperCache.h"
#include "mozilla/RefPtr.h"

class nsINode;

namespace mozilla {

namespace a11y {
  class Accessible;
}

namespace dom {

struct ParentObject;

class AccessibleNode : public nsISupports,
                       public nsWrapperCache
{
public:
  explicit AccessibleNode(nsINode* aNode);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS;
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(AccessibleNode);

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override final;
  virtual dom::ParentObject GetParentObject() const final;

  void GetRole(nsAString& aRole);
  nsINode* GetDOMNode();

protected:
  AccessibleNode(const AccessibleNode& aCopy) = delete;
  AccessibleNode& operator=(const AccessibleNode& aCopy) = delete;
  virtual ~AccessibleNode();

  RefPtr<a11y::Accessible> mIntl;
  RefPtr<nsINode> mDOMNode;
};

} // dom
} // mozilla


#endif // A11Y_JSAPI_ACCESSIBLENODE
