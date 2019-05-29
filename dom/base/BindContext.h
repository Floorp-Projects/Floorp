/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* State that is passed down to BindToTree. */

#ifndef mozilla_dom_BindContext_h__
#define mozilla_dom_BindContext_h__

#include "mozilla/Attributes.h"
#include "nsXBLBinding.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ShadowRoot.h"

namespace mozilla {
namespace dom {

struct MOZ_STACK_CLASS BindContext final {
  // Whether our subtree root is changing as a result of this operation.
  bool SubtreeRootChanges() const { return mSubtreeRootChanges; }

  // Returns the binding parent of the subtree to be inserted.
  //
  // This can be null.
  Element* GetBindingParent() const { return mBindingParent; }

  // This constructor should be used for regular appends to content.
  //
  // FIXME(emilio, bug 1555944): nsIContent::GetBindingParent() should return an
  // Element*.
  explicit BindContext(nsINode& aParentNode)
      : mSubtreeRootChanges(true),
        mBindingParent(aParentNode.IsContent()
                           ? static_cast<Element*>(
                                 aParentNode.AsContent()->GetBindingParent())
                           : nullptr) {}

  // When re-binding a shadow host into a tree, we re-bind all the shadow tree
  // from the root. In that case, the shadow tree contents remain within the
  // same subtree root.  So children should avoid doing silly things like adding
  // themselves to the ShadowRoot's id table twice or what not.
  //
  // This constructor is only meant to be used in that situation.
  explicit BindContext(ShadowRoot& aShadowRoot)
      : mSubtreeRootChanges(false),
        mBindingParent(aShadowRoot.Host()) {}

  // This constructor is meant to be used when inserting native-anonymous
  // children into a subtree.
  enum ForNativeAnonymous { ForNativeAnonymous };
  BindContext(Element& aParentElement, enum ForNativeAnonymous)
      : mSubtreeRootChanges(true),
        mBindingParent(&aParentElement) {}

  // This is meant to be used to bind XBL anonymous content.
  BindContext(nsXBLBinding& aBinding, Element& aParentElement)
      : mSubtreeRootChanges(true),
        mBindingParent(aBinding.GetBoundElement()) {}

 private:
  // Whether the bind operation will change the subtree root of the content
  // we're binding.
  const bool mSubtreeRootChanges;
  Element* const mBindingParent;
};

}  // namespace dom
}  // namespace mozilla

#endif
