/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_base_GeneratedImageContent_h
#define dom_base_GeneratedImageContent_h

/* A content node that keeps track of an index in the parent's `content`
 * property value, used for url() values in the content of a ::before or ::after
 * pseudo-element. */

#include "mozilla/dom/HTMLElementBinding.h"
#include "mozilla/dom/NameSpaceConstants.h"
#include "mozilla/dom/NodeInfo.h"
#include "nsGenericHTMLElement.h"

namespace mozilla::dom {

class GeneratedImageContent final : public nsGenericHTMLElement {
 public:
  static already_AddRefed<GeneratedImageContent> Create(Document&,
                                                        uint32_t aContentIndex);
  // An image created from 'list-style-image' for a ::marker pseudo.
  static already_AddRefed<GeneratedImageContent> CreateForListStyleImage(
      Document&);

  explicit GeneratedImageContent(already_AddRefed<dom::NodeInfo>&& aNodeInfo)
      : nsGenericHTMLElement(std::move(aNodeInfo)) {
    MOZ_ASSERT(IsInNamespace(kNameSpaceID_XHTML),
               "Someone messed up our nodeinfo");
  }

  nsresult Clone(dom::NodeInfo* aNodeInfo, nsINode** aResult) const final;

  nsresult CopyInnerTo(GeneratedImageContent* aDest) {
    nsresult rv = nsGenericHTMLElement::CopyInnerTo(aDest);
    NS_ENSURE_SUCCESS(rv, rv);
    aDest->mIndex = mIndex;
    return NS_OK;
  }

  // Is this an image created from 'list-style-image'?
  bool IsForListStyleImageMarker() const { return Index() == uint32_t(-1); }

  // @note we use -1 for images created from 'list-style-image'
  uint32_t Index() const { return mIndex; }

  // Notify this image failed to load.
  void NotifyLoadFailed() { SetStates(ElementState::BROKEN, true); }

 protected:
  JSObject* WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;

 private:
  virtual ~GeneratedImageContent() = default;
  uint32_t mIndex = 0;
};

}  // namespace mozilla::dom

#endif  // dom_base_GeneratedImageContent_h
