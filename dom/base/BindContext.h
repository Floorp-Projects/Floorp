/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* State that is passed down to BindToTree. */

#ifndef mozilla_dom_BindContext_h__
#define mozilla_dom_BindContext_h__

#include "mozilla/Attributes.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ShadowRoot.h"
#include "nsINode.h"

namespace mozilla::dom {

class Document;

struct MOZ_STACK_CLASS BindContext final {
  // The document that owns the tree we're getting bound to.
  //
  // This is mostly an optimization to avoid silly pointer-chases to get the
  // OwnerDoc().
  Document& OwnerDoc() const { return mDoc; }

  // Whether we're getting connected.
  //
  // https://dom.spec.whatwg.org/#connected
  bool InComposedDoc() const { return mInComposedDoc; }

  // Whether we're getting bound to the document tree.
  //
  // https://dom.spec.whatwg.org/#in-a-document-tree
  bool InUncomposedDoc() const { return mInUncomposedDoc; }

  Document* GetComposedDoc() const { return mInComposedDoc ? &mDoc : nullptr; }

  Document* GetUncomposedDoc() const {
    return mInUncomposedDoc ? &mDoc : nullptr;
  }

  // Whether our subtree root is changing as a result of this operation.
  bool SubtreeRootChanges() const { return mSubtreeRootChanges; }

  // Autofocus is allowed only if the is in the same origin as the top level
  // document.
  // https://html.spec.whatwg.org/multipage/interaction.html#the-autofocus-attribute:same-origin
  // In addition, the document should not be already loaded and the
  // "browser.autofocus" preference should be 'true'.
  bool AllowsAutoFocus() const;

  // This constructor should be used for regular appends to content.
  explicit BindContext(nsINode& aParent)
      : mDoc(*aParent.OwnerDoc()),
        mInComposedDoc(aParent.IsInComposedDoc()),
        mInUncomposedDoc(aParent.IsInUncomposedDoc()),
        mSubtreeRootChanges(true) {}

  // When re-binding a shadow host into a tree, we re-bind all the shadow tree
  // from the root. In that case, the shadow tree contents remain within the
  // same subtree root.  So children should avoid doing silly things like adding
  // themselves to the ShadowRoot's id table twice or what not.
  //
  // This constructor is only meant to be used in that situation.
  explicit BindContext(ShadowRoot& aShadowRoot)
      : mDoc(*aShadowRoot.OwnerDoc()),
        mInComposedDoc(aShadowRoot.IsInComposedDoc()),
        mInUncomposedDoc(false),
        mSubtreeRootChanges(false) {}

  // This constructor is meant to be used when inserting native-anonymous
  // children into a subtree.
  enum ForNativeAnonymous { ForNativeAnonymous };
  BindContext(Element& aParentElement, enum ForNativeAnonymous)
      : mDoc(*aParentElement.OwnerDoc()),
        mInComposedDoc(aParentElement.IsInComposedDoc()),
        mInUncomposedDoc(aParentElement.IsInUncomposedDoc()),
        mSubtreeRootChanges(true) {
    MOZ_ASSERT(mInComposedDoc, "Binding NAC in a disconnected subtree?");
  }

 private:
  // Returns true iff the document is in the same origin as the top level
  // document.
  bool IsSameOriginAsTop() const;

  Document& mDoc;

  const bool mInComposedDoc;
  const bool mInUncomposedDoc;

  // Whether the bind operation will change the subtree root of the content
  // we're binding.
  const bool mSubtreeRootChanges;
};

}  // namespace mozilla::dom

#endif
