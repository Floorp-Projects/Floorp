/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LayersTypes.h"

#include "nsStyleStruct.h"              // for nsStyleFilter

namespace mozilla {
namespace layers {

CSSFilter ToCSSFilter(const nsStyleFilter& filter)
{
  switch (filter.GetType()) {
    case NS_STYLE_FILTER_BRIGHTNESS: {
      return {
          CSSFilterType::BRIGHTNESS,
          filter.GetFilterParameter().GetFactorOrPercentValue(),
      };
    }
    case NS_STYLE_FILTER_CONTRAST: {
      return {
          CSSFilterType::CONTRAST,
          filter.GetFilterParameter().GetFactorOrPercentValue(),
      };
    }
    case NS_STYLE_FILTER_GRAYSCALE: {
      return {
          CSSFilterType::GRAYSCALE,
          filter.GetFilterParameter().GetFactorOrPercentValue(),
      };
    }
    case NS_STYLE_FILTER_INVERT: {
      return {
          CSSFilterType::INVERT,
          filter.GetFilterParameter().GetFactorOrPercentValue(),
      };
    }
    case NS_STYLE_FILTER_SEPIA: {
      return {
          CSSFilterType::SEPIA,
          filter.GetFilterParameter().GetFactorOrPercentValue(),
      };
    }
    // All other filter types should be prevented by the code which converts
    // display items into layers.
    default:
      MOZ_ASSERT_UNREACHABLE("Tried to convert an unsupported filter");
      return { CSSFilterType::CONTRAST, 0 };
  }
}

} // namespace layers
} // namespace mozilla

