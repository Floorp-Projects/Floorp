/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ViewTimeline_h
#define mozilla_dom_ViewTimeline_h

#include "mozilla/dom/ScrollTimeline.h"

namespace mozilla {
class ScrollContainerFrame;
}  // namespace mozilla

namespace mozilla::dom {

/*
 * A view progress timeline is a segment of a scroll progress timeline that are
 * scoped to the scroll positions in which any part of the associated element’s
 * principal box intersects its nearest ancestor scrollport. So ViewTimeline
 * is a special case of ScrollTimeline.
 */
class ViewTimeline final : public ScrollTimeline {
  template <typename T, typename... Args>
  friend already_AddRefed<T> mozilla::MakeAndAddRef(Args&&... aArgs);

 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ViewTimeline, ScrollTimeline)

  ViewTimeline() = delete;

  // Note: |aSubject| is used as the subject which specifies view-timeline-name
  // property, and we use this subject to look up its nearest scroll container.
  static already_AddRefed<ViewTimeline> MakeNamed(
      Document* aDocument, Element* aSubject, PseudoStyleType aPseudoType,
      const StyleViewTimeline& aStyleTimeline);

  static already_AddRefed<ViewTimeline> MakeAnonymous(
      Document* aDocument, const NonOwningAnimationTarget& aTarget,
      StyleScrollAxis aAxis, const StyleViewTimelineInset& aInset);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override {
    return nullptr;
  }

  bool IsViewTimeline() const override { return true; }

  void ReplacePropertiesWith(Element* aSubjectElement,
                             PseudoStyleType aPseudoType,
                             const StyleViewTimeline& aNew);

 private:
  ~ViewTimeline() = default;
  ViewTimeline(Document* aDocument, const Scroller& aScroller,
               StyleScrollAxis aAxis, Element* aSubject,
               PseudoStyleType aSubjectPseudoType,
               const StyleViewTimelineInset& aInset)
      : ScrollTimeline(aDocument, aScroller, aAxis),
        mSubject(aSubject),
        mSubjectPseudoType(aSubjectPseudoType),
        mInset(aInset) {}

  Maybe<ScrollOffsets> ComputeOffsets(
      const ScrollContainerFrame* aScrollContainerFrame,
      layers::ScrollDirection aOrientation) const override;

  ScrollOffsets ComputeInsets(const ScrollContainerFrame* aScrollContainerFrame,
                              layers::ScrollDirection aOrientation) const;

  // The subject element.
  // 1. For view(), the subject element is the animation target.
  // 2. For view-timeline property, the subject element is the element who
  //    defines this property.
  RefPtr<Element> mSubject;
  PseudoStyleType mSubjectPseudoType;

  // FIXME: Bug 1817073. view-timeline-inset is an animatable property. However,
  // the inset from view() is not animatable, so for named view timeline, this
  // value depends on the animation style. Therefore, we have to check its style
  // value when using it. For now, in order to simplify the implementation, we
  // make |mInset| be fixed.
  StyleViewTimelineInset mInset;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_ViewTimeline_h
