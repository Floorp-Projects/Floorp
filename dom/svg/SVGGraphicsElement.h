/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGGRAPHICSELEMENT_H_
#define DOM_SVG_SVGGRAPHICSELEMENT_H_

#include "mozilla/dom/SVGTests.h"
#include "mozilla/dom/SVGTransformableElement.h"

namespace mozilla::dom {

using SVGGraphicsElementBase = SVGTransformableElement;

class SVGGraphicsElement : public SVGGraphicsElementBase, public SVGTests {
 protected:
  explicit SVGGraphicsElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  ~SVGGraphicsElement() = default;

 public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  NS_IMPL_FROMNODE_HELPER(SVGGraphicsElement, IsSVGGraphicsElement())

  // WebIDL
  SVGElement* GetNearestViewportElement();
  SVGElement* GetFarthestViewportElement();
  MOZ_CAN_RUN_SCRIPT
  already_AddRefed<SVGRect> GetBBox(const SVGBoundingBoxOptions&);
  already_AddRefed<SVGMatrix> GetCTM();
  already_AddRefed<SVGMatrix> GetScreenCTM();

  Focusable IsFocusableWithoutStyle(bool aWithMouse) override;
  bool IsSVGGraphicsElement() const final { return true; }

  using nsINode::Clone;
  // Overrides SVGTests.
  bool PassesConditionalProcessingTests() const final {
    return SVGTests::PassesConditionalProcessingTests();
  }
  SVGElement* AsSVGElement() final { return this; }

 protected:
  // returns true if focusability has been definitively determined otherwise
  // false
  bool IsSVGFocusable(bool* aIsFocusable, int32_t* aTabIndex);

  template <typename T>
  bool IsInLengthInfo(const nsAtom* aAttribute, const T& aLengthInfos) const {
    for (auto const& e : aLengthInfos) {
      if (e.mName == aAttribute) {
        return true;
      }
    }
    return false;
  }
};

}  // namespace mozilla::dom

#endif  // DOM_SVG_SVGGRAPHICSELEMENT_H_
