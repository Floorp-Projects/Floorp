/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGSwitchElement.h"

#include "nsLayoutUtils.h"
#include "mozilla/SVGUtils.h"
#include "mozilla/dom/SVGSwitchElementBinding.h"

class nsIFrame;

NS_IMPL_NS_NEW_SVG_ELEMENT(Switch)

namespace mozilla::dom {

JSObject* SVGSwitchElement::WrapNode(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return SVGSwitchElement_Binding::Wrap(aCx, this, aGivenProto);
}

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_CYCLE_COLLECTION_INHERITED(SVGSwitchElement, SVGSwitchElementBase,
                                   mActiveChild)

NS_IMPL_ADDREF_INHERITED(SVGSwitchElement, SVGSwitchElementBase)
NS_IMPL_RELEASE_INHERITED(SVGSwitchElement, SVGSwitchElementBase)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SVGSwitchElement)
NS_INTERFACE_MAP_END_INHERITING(SVGSwitchElementBase)

//----------------------------------------------------------------------
// Implementation

SVGSwitchElement::SVGSwitchElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : SVGSwitchElementBase(std::move(aNodeInfo)) {}

void SVGSwitchElement::MaybeInvalidate() {
  // We must not change mActiveChild until after
  // InvalidateAndScheduleBoundsUpdate has been called, otherwise
  // it will not correctly invalidate the old mActiveChild area.

  auto* newActiveChild = SVGTests::FindActiveSwitchChild(this);

  if (newActiveChild == mActiveChild) {
    return;
  }

  if (auto* frame = GetPrimaryFrame()) {
    nsLayoutUtils::PostRestyleEvent(this, RestyleHint{0},
                                    nsChangeHint_InvalidateRenderingObservers);
    SVGUtils::ScheduleReflowSVG(frame);
  }

  mActiveChild = newActiveChild;
}

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGSwitchElement)

//----------------------------------------------------------------------
// nsINode methods

void SVGSwitchElement::InsertChildBefore(nsIContent* aKid,
                                         nsIContent* aBeforeThis, bool aNotify,
                                         ErrorResult& aRv) {
  SVGSwitchElementBase::InsertChildBefore(aKid, aBeforeThis, aNotify, aRv);
  if (aRv.Failed()) {
    return;
  }

  MaybeInvalidate();
}

void SVGSwitchElement::RemoveChildNode(nsIContent* aKid, bool aNotify) {
  SVGSwitchElementBase::RemoveChildNode(aKid, aNotify);
  MaybeInvalidate();
}

}  // namespace mozilla::dom
