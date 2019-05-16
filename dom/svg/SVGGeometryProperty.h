/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGGeometryProperty_SVGGeometryProperty_h
#define mozilla_dom_SVGGeometryProperty_SVGGeometryProperty_h

#include "mozilla/dom/SVGElement.h"
#include "SVGAnimatedLength.h"
#include "ComputedStyle.h"
#include "nsIFrame.h"
#include <type_traits>

namespace mozilla {
namespace dom {

namespace SVGGeometryProperty {
namespace ResolverTypes {
struct LengthPercentNoAuto {};
struct LengthPercentRXY {};
struct LengthPercentWidthHeight {};
}  // namespace ResolverTypes

namespace Tags {

#define SVGGEOMETRYPROPERTY_GENERATETAG(tagName, resolver, direction, \
                                        styleStruct)                  \
  struct tagName {                                                    \
    using ResolverType = ResolverTypes::resolver;                     \
    constexpr static auto CtxDirection = SVGContentUtils::direction;  \
    constexpr static auto Getter = &styleStruct::m##tagName;          \
  }

SVGGEOMETRYPROPERTY_GENERATETAG(X, LengthPercentNoAuto, X, nsStyleSVGReset);
SVGGEOMETRYPROPERTY_GENERATETAG(Y, LengthPercentNoAuto, Y, nsStyleSVGReset);
SVGGEOMETRYPROPERTY_GENERATETAG(Cx, LengthPercentNoAuto, X, nsStyleSVGReset);
SVGGEOMETRYPROPERTY_GENERATETAG(Cy, LengthPercentNoAuto, Y, nsStyleSVGReset);
SVGGEOMETRYPROPERTY_GENERATETAG(R, LengthPercentNoAuto, XY, nsStyleSVGReset);
SVGGEOMETRYPROPERTY_GENERATETAG(Width, LengthPercentWidthHeight, X,
                                nsStylePosition);
SVGGEOMETRYPROPERTY_GENERATETAG(Height, LengthPercentWidthHeight, Y,
                                nsStylePosition);

#undef SVGGEOMETRYPROPERTY_GENERATETAG

struct Ry;
struct Rx {
  using ResolverType = ResolverTypes::LengthPercentRXY;
  constexpr static auto CtxDirection = SVGContentUtils::X;
  constexpr static auto Getter = &nsStyleSVGReset::mRx;
  using CounterPart = Ry;
};
struct Ry {
  using ResolverType = ResolverTypes::LengthPercentRXY;
  constexpr static auto CtxDirection = SVGContentUtils::Y;
  constexpr static auto Getter = &nsStyleSVGReset::mRy;
  using CounterPart = Rx;
};

}  // namespace Tags

namespace details {
template <class T>
using AlwaysFloat = float;

using CtxDirectionType = decltype(SVGContentUtils::X);

template <CtxDirectionType CTD>
float ResolvePureLengthPercentage(SVGElement* aElement,
                                  const LengthPercentage& aLP) {
  return aLP.ResolveToCSSPixelsWith(
      [&] { return CSSCoord{SVGElementMetrics(aElement).GetAxisLength(CTD)}; });
}

template <class Tag>
float ResolveImpl(ComputedStyle const& aStyle, SVGElement* aElement,
                  ResolverTypes::LengthPercentNoAuto) {
  auto const& value = aStyle.StyleSVGReset()->*Tag::Getter;
  return ResolvePureLengthPercentage<Tag::CtxDirection>(aElement, value);
}

template <class Tag>
float ResolveImpl(ComputedStyle const& aStyle, SVGElement* aElement,
                  ResolverTypes::LengthPercentWidthHeight) {
  static_assert(
      std::is_same<Tag, Tags::Width>{} || std::is_same<Tag, Tags::Height>{},
      "Wrong tag");

  auto const& value = aStyle.StylePosition()->*Tag::Getter;
  if (value.IsLengthPercentage()) {
    return ResolvePureLengthPercentage<Tag::CtxDirection>(
        aElement, value.AsLengthPercentage());
  }

  // |auto| and |max-content| etc. are treated as 0.
  return 0.f;
}

template <class Tag>
float ResolveImpl(ComputedStyle const& aStyle, SVGElement* aElement,
                  ResolverTypes::LengthPercentRXY) {
  static_assert(std::is_same<Tag, Tags::Rx>{} || std::is_same<Tag, Tags::Ry>{},
                "Wrong tag");

  auto const& value = aStyle.StyleSVGReset()->*Tag::Getter;
  if (value.IsLengthPercentage()) {
    return ResolvePureLengthPercentage<Tag::CtxDirection>(
        aElement, value.AsLengthPercentage());
  }

  MOZ_ASSERT(value.IsAuto());
  using Rother = typename Tag::CounterPart;
  auto const& valueOther = aStyle.StyleSVGReset()->*Rother::Getter;

  if (valueOther.IsAuto()) {
    // Per SVG2, |Rx|, |Ry| resolve to 0 if both are |auto|
    return 0.f;
  }

  // If |Rx| is auto while |Ry| not, |Rx| gets the value of |Ry|.
  return ResolvePureLengthPercentage<Rother::CtxDirection>(
      aElement, valueOther.AsLengthPercentage());
}

}  // namespace details

template <class Tag>
float ResolveWith(const ComputedStyle& aStyle, const SVGElement* aElement) {
  // TODO: There are a lot of utilities lacking const-ness in dom/svg.
  // We should fix that problem and remove this `const_cast`.
  return details::ResolveImpl<Tag>(aStyle, const_cast<SVGElement*>(aElement),
                                   typename Tag::ResolverType{});
}

// To add support for new properties, or to handle special cases for
// existing properties, you can add a new tag in |Tags| and |ResolverTypes|
// namespace, then implement the behavior in |details::ResolveImpl|.
template <class... Tags>
bool ResolveAll(const SVGElement* aElement,
                details::AlwaysFloat<Tags>*... aRes) {
  if (nsIFrame const* f = aElement->GetPrimaryFrame()) {
    using dummy = int[];
    (void)dummy{0, (*aRes = ResolveWith<Tags>(*f->Style(), aElement), 0)...};
    return true;
  }
  return false;
}

nsCSSUnit SpecifiedUnitTypeToCSSUnit(uint8_t aSpecifiedUnit);
nsCSSPropertyID AttrEnumToCSSPropId(const SVGElement* aElement,
                                    uint8_t aAttrEnum);

bool IsNonNegativeGeometryProperty(nsCSSPropertyID aProp);
bool ElementMapsLengthsToStyle(SVGElement const* aElement);

}  // namespace SVGGeometryProperty
}  // namespace dom
}  // namespace mozilla

#endif
