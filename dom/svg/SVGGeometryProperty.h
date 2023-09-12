/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGGEOMETRYPROPERTY_H_
#define DOM_SVG_SVGGEOMETRYPROPERTY_H_

#include "mozilla/SVGImageFrame.h"
#include "mozilla/dom/SVGElement.h"
#include "ComputedStyle.h"
#include "SVGAnimatedLength.h"
#include "nsComputedDOMStyle.h"
#include "nsGkAtoms.h"
#include "nsIFrame.h"
#include <type_traits>

namespace mozilla::dom::SVGGeometryProperty {
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

#undef SVGGEOMETRYPROPERTY_GENERATETAG

struct Height;
struct Width {
  using ResolverType = ResolverTypes::LengthPercentWidthHeight;
  constexpr static auto CtxDirection = SVGContentUtils::X;
  constexpr static auto Getter = &nsStylePosition::mWidth;
  constexpr static auto SizeGetter = &gfx::Size::width;
  static AspectRatio AspectRatioRelative(AspectRatio aAspectRatio) {
    return aAspectRatio.Inverted();
  }
  constexpr static uint32_t DefaultObjectSize = 300;
  using CounterPart = Height;
};
struct Height {
  using ResolverType = ResolverTypes::LengthPercentWidthHeight;
  constexpr static auto CtxDirection = SVGContentUtils::Y;
  constexpr static auto Getter = &nsStylePosition::mHeight;
  constexpr static auto SizeGetter = &gfx::Size::height;
  static AspectRatio AspectRatioRelative(AspectRatio aAspectRatio) {
    return aAspectRatio;
  }
  constexpr static uint32_t DefaultObjectSize = 150;
  using CounterPart = Width;
};

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
using dummy = int[];

using CtxDirectionType = decltype(SVGContentUtils::X);

template <CtxDirectionType CTD>
float ResolvePureLengthPercentage(const SVGElement* aElement,
                                  const LengthPercentage& aLP) {
  return aLP.ResolveToCSSPixelsWith(
      [&] { return CSSCoord{SVGElementMetrics(aElement).GetAxisLength(CTD)}; });
}

template <class Tag>
float ResolveImpl(ComputedStyle const& aStyle, const SVGElement* aElement,
                  ResolverTypes::LengthPercentNoAuto) {
  auto const& value = aStyle.StyleSVGReset()->*Tag::Getter;
  return ResolvePureLengthPercentage<Tag::CtxDirection>(aElement, value);
}

template <class Tag>
float ResolveImpl(ComputedStyle const& aStyle, const SVGElement* aElement,
                  ResolverTypes::LengthPercentWidthHeight) {
  static_assert(
      std::is_same<Tag, Tags::Width>{} || std::is_same<Tag, Tags::Height>{},
      "Wrong tag");

  auto const& value = aStyle.StylePosition()->*Tag::Getter;
  if (value.IsLengthPercentage()) {
    return ResolvePureLengthPercentage<Tag::CtxDirection>(
        aElement, value.AsLengthPercentage());
  }

  if (aElement->IsSVGElement(nsGkAtoms::image)) {
    // It's not clear per SVG2 spec what should be done for values other
    // than |auto| (e.g. |max-content|). We treat them as nonsense, thus
    // using the initial value behavior, i.e. |auto|.
    // The following procedure follows the Default Sizing Algorithm as
    // specified in:
    // https://svgwg.org/svg2-draft/embedded.html#ImageElement

    SVGImageFrame* imgf = do_QueryFrame(aElement->GetPrimaryFrame());
    if (!imgf) {
      return 0.f;
    }

    using Other = typename Tag::CounterPart;
    auto const& valueOther = aStyle.StylePosition()->*Other::Getter;

    gfx::Size intrinsicImageSize;
    AspectRatio aspectRatio;
    if (!imgf->GetIntrinsicImageDimensions(intrinsicImageSize, aspectRatio)) {
      // No image container, just return 0.
      return 0.f;
    }

    if (valueOther.IsLengthPercentage()) {
      // We are |auto|, but the other side has specifed length.
      float lengthOther = ResolvePureLengthPercentage<Other::CtxDirection>(
          aElement, valueOther.AsLengthPercentage());

      if (aspectRatio) {
        // Preserve aspect ratio if it's present.
        return Other::AspectRatioRelative(aspectRatio)
            .ApplyToFloat(lengthOther);
      }

      float intrinsicLength = intrinsicImageSize.*Tag::SizeGetter;
      if (intrinsicLength >= 0) {
        // Use the intrinsic length if it's present.
        return intrinsicLength;
      }

      // No specified size, no aspect ratio, no intrinsic length,
      // then use default size.
      return Tag::DefaultObjectSize;
    }

    // |width| and |height| are both |auto|
    if (intrinsicImageSize.*Tag::SizeGetter >= 0) {
      return intrinsicImageSize.*Tag::SizeGetter;
    }

    if (intrinsicImageSize.*Other::SizeGetter >= 0 && aspectRatio) {
      return Other::AspectRatioRelative(aspectRatio)
          .ApplyTo(intrinsicImageSize.*Other::SizeGetter);
    }

    if (aspectRatio) {
      // Resolve as a contain constraint against the default object size.
      auto defaultAspectRatioRelative =
          AspectRatio{float(Other::DefaultObjectSize) / Tag::DefaultObjectSize};
      auto aspectRatioRelative = Tag::AspectRatioRelative(aspectRatio);

      if (defaultAspectRatioRelative < aspectRatioRelative) {
        // Using default length in our side and the intrinsic aspect ratio,
        // the other side cannot be contained.
        return aspectRatioRelative.Inverted().ApplyTo(Other::DefaultObjectSize);
      }

      return Tag::DefaultObjectSize;
    }

    return Tag::DefaultObjectSize;
  }

  // For other elements, |auto| and |max-content| etc. are treated as 0.
  return 0.f;
}

template <class Tag>
float ResolveImpl(ComputedStyle const& aStyle, const SVGElement* aElement,
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
  return details::ResolveImpl<Tag>(aStyle, aElement,
                                   typename Tag::ResolverType{});
}

template <class Func>
bool DoForComputedStyle(const Element* aElement, Func aFunc) {
  if (!aElement) {
    return false;
  }
  if (const nsIFrame* f = aElement->GetPrimaryFrame()) {
    aFunc(f->Style());
    return true;
  }

  if (RefPtr<const ComputedStyle> computedStyle =
          nsComputedDOMStyle::GetComputedStyleNoFlush(aElement)) {
    aFunc(computedStyle.get());
    return true;
  }

  return false;
}

#define SVGGEOMETRYPROPERTY_EVAL_ALL(expr) \
  (void)details::dummy { 0, (static_cast<void>(expr), 0)... }

// To add support for new properties, or to handle special cases for
// existing properties, you can add a new tag in |Tags| and |ResolverTypes|
// namespace, then implement the behavior in |details::ResolveImpl|.
template <class... Tags>
bool ResolveAll(const SVGElement* aElement,
                details::AlwaysFloat<Tags>*... aRes) {
  bool res = DoForComputedStyle(aElement, [&](auto const* style) {
    SVGGEOMETRYPROPERTY_EVAL_ALL(*aRes = ResolveWith<Tags>(*style, aElement));
  });

  if (res) {
    return true;
  }

  SVGGEOMETRYPROPERTY_EVAL_ALL(*aRes = 0);
  return false;
}

#undef SVGGEOMETRYPROPERTY_EVAL_ALL

nsCSSUnit SpecifiedUnitTypeToCSSUnit(uint8_t aSpecifiedUnit);
nsCSSPropertyID AttrEnumToCSSPropId(const SVGElement* aElement,
                                    uint8_t aAttrEnum);

bool IsNonNegativeGeometryProperty(nsCSSPropertyID aProp);
bool ElementMapsLengthsToStyle(SVGElement const* aElement);

}  // namespace mozilla::dom::SVGGeometryProperty

#endif  // DOM_SVG_SVGGEOMETRYPROPERTY_H_
