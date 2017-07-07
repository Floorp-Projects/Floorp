/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SVGTRANSFORMLISTPARSER_H__
#define MOZILLA_SVGTRANSFORMLISTPARSER_H__

#include "mozilla/Attributes.h"
#include "nsSVGDataParser.h"
#include "nsTArray.h"

////////////////////////////////////////////////////////////////////////
// SVGTransformListParser: A simple recursive descent parser that builds
// transform lists from transform attributes. The grammar for path data
// can be found in SVG 1.1,  chapter 7.
// http://www.w3.org/TR/SVG11/coords.html#TransformAttribute

namespace mozilla {

class nsSVGTransform;

class SVGTransformListParser : public nsSVGDataParser
{
public:
  explicit SVGTransformListParser(const nsAString& aValue)
    : nsSVGDataParser(aValue) {}

  bool Parse();

  const nsTArray<nsSVGTransform>& GetTransformList() const {
    return mTransforms;
  }

private:
  // helpers
  bool ParseArguments(float *aResult,
                      uint32_t aMaxCount,
                      uint32_t *aParsedCount);

  bool ParseTransforms();

  bool ParseTransform();

  bool ParseTranslate();
  bool ParseScale();
  bool ParseRotate();
  bool ParseSkewX();
  bool ParseSkewY();
  bool ParseMatrix();

  FallibleTArray<nsSVGTransform> mTransforms;
};

} // namespace mozilla

#endif // MOZILLA_SVGTRANSFORMLISTPARSER_H__
