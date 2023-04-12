/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ViewTimeline.h"

#include "mozilla/dom/Animation.h"
#include "mozilla/dom/ElementInlines.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(ViewTimeline, ScrollTimeline, mSubject)
NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(ViewTimeline, ScrollTimeline)

/* static */
already_AddRefed<ViewTimeline> ViewTimeline::MakeNamed(
    Document* aDocument, Element* aSubject, PseudoStyleType aPseudoType,
    const StyleViewTimeline& aStyleTimeline) {
  MOZ_ASSERT(NS_IsMainThread());

  // 1. Lookup scroller. We have to find the nearest scroller from |aSubject|
  // and |aPseudoType|.
  auto [element, pseudo] = FindNearestScroller(aSubject, aPseudoType);
  auto scroller = Scroller::Nearest(const_cast<Element*>(element), pseudo);

  // 2. Create timeline.
  return RefPtr<ViewTimeline>(
             new ViewTimeline(aDocument, scroller, aStyleTimeline.GetAxis(),
                              aSubject, aPseudoType, aStyleTimeline.GetInset()))
      .forget();
}

void ViewTimeline::ReplacePropertiesWith(Element* aSubjectElement,
                                         PseudoStyleType aPseudoType,
                                         const StyleViewTimeline& aNew) {
  mSubject = aSubjectElement;
  mSubjectPseudoType = aPseudoType;
  mAxis = aNew.GetAxis();
  // FIXME: Bug 1817073. We assume it is a non-animatable value for now.
  mInset = aNew.GetInset();

  for (auto* anim = mAnimationOrder.getFirst(); anim;
       anim = static_cast<LinkedListElement<Animation>*>(anim)->getNext()) {
    MOZ_ASSERT(anim->GetTimeline() == this);
    // Set this so we just PostUpdate() for this animation.
    anim->SetTimeline(this);
  }
}

}  // namespace mozilla::dom
