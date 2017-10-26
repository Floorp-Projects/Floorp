/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "SVGTransformListParser.h"
#include "SVGContentUtils.h"
#include "nsSVGTransform.h"
#include "nsGkAtoms.h"
#include "nsAtom.h"

using namespace mozilla;

//----------------------------------------------------------------------
// private methods

bool
SVGTransformListParser::Parse()
{
  mTransforms.Clear();
  return ParseTransforms();
}

bool
SVGTransformListParser::ParseTransforms()
{
  if (!SkipWsp()) {
    return true;
  }

  if (!ParseTransform()) {
    return false;
  }

  while (SkipWsp()) {
    // The SVG BNF allows multiple comma-wsp between transforms
    while (*mIter == ',') {
      ++mIter;
      if (!SkipWsp()) {
        return false;
      }
    }

    if (!ParseTransform()) {
      return false;
    }
  }
  return true;
}

bool
SVGTransformListParser::ParseTransform()
{
  RangedPtr<const char16_t> start(mIter);
  while (IsAlpha(*mIter)) {
    ++mIter;
    if (mIter == mEnd) {
      return false;
    }
  }

  if (start == mIter) {
    // Didn't read anything
    return false;
  }

  const nsAString& transform = Substring(start.get(), mIter.get());
  nsStaticAtom* keyAtom = NS_GetStaticAtom(transform);

  if (!keyAtom || !SkipWsp()) {
    return false;
  }

  if (keyAtom == nsGkAtoms::translate) {
    return ParseTranslate();
  }
  if (keyAtom == nsGkAtoms::scale) {
    return ParseScale();
  }
  if (keyAtom == nsGkAtoms::rotate) {
    return ParseRotate();
  }
  if (keyAtom == nsGkAtoms::skewX) {
    return ParseSkewX();
  }
  if (keyAtom == nsGkAtoms::skewY) {
    return ParseSkewY();
  }
  if (keyAtom == nsGkAtoms::matrix) {
    return ParseMatrix();
  }
  return false;
}

bool
SVGTransformListParser::ParseArguments(float* aResult,
                                       uint32_t aMaxCount,
                                       uint32_t* aParsedCount)
{
  if (*mIter != '(') {
    return false;
  }
  ++mIter;

  if (!SkipWsp()) {
    return false;
  }

  if (!SVGContentUtils::ParseNumber(mIter, mEnd, aResult[0])) {
    return false;
  }
  *aParsedCount = 1;

  while (SkipWsp()) {
    if (*mIter == ')') {
      ++mIter;
      return true;
    }
    if (*aParsedCount == aMaxCount) {
      return false;
    }
    SkipCommaWsp();
    if (!SVGContentUtils::ParseNumber(mIter, mEnd, aResult[(*aParsedCount)++])) {
      return false;
    }
  }
  return false;
}

bool
SVGTransformListParser::ParseTranslate()
{
  float t[2];
  uint32_t count;

  if (!ParseArguments(t, ArrayLength(t), &count)) {
    return false;
  }

  switch (count) {
    case 1:
      t[1] = 0.f;
      MOZ_FALLTHROUGH;
    case 2:
    {
      nsSVGTransform* transform = mTransforms.AppendElement(fallible);
      if (!transform) {
        return false;
      }
      transform->SetTranslate(t[0], t[1]);
      return true;
    }
  }

  return false;
}

bool
SVGTransformListParser::ParseScale()
{
  float s[2];
  uint32_t count;

  if (!ParseArguments(s, ArrayLength(s), &count)) {
    return false;
  }

  switch (count) {
    case 1:
      s[1] = s[0];
      MOZ_FALLTHROUGH;
    case 2:
    {
      nsSVGTransform* transform = mTransforms.AppendElement(fallible);
      if (!transform) {
        return false;
      }
      transform->SetScale(s[0], s[1]);
      return true;
    }
  }

  return false;
}


bool
SVGTransformListParser::ParseRotate()
{
  float r[3];
  uint32_t count;

  if (!ParseArguments(r, ArrayLength(r), &count)) {
    return false;
  }

  switch (count) {
    case 1:
      r[1] = r[2] = 0.f;
      MOZ_FALLTHROUGH;
    case 3:
    {
      nsSVGTransform* transform = mTransforms.AppendElement(fallible);
      if (!transform) {
        return false;
      }
      transform->SetRotate(r[0], r[1], r[2]);
      return true;
    }
  }

  return false;
}

bool
SVGTransformListParser::ParseSkewX()
{
  float skew;
  uint32_t count;

  if (!ParseArguments(&skew, 1, &count) || count != 1) {
    return false;
  }

  nsSVGTransform* transform = mTransforms.AppendElement(fallible);
  if (!transform) {
    return false;
  }
  transform->SetSkewX(skew);

  return true;
}

bool
SVGTransformListParser::ParseSkewY()
{
  float skew;
  uint32_t count;

  if (!ParseArguments(&skew, 1, &count) || count != 1) {
    return false;
  }

  nsSVGTransform* transform = mTransforms.AppendElement(fallible);
  if (!transform) {
    return false;
  }
  transform->SetSkewY(skew);

  return true;
}

bool
SVGTransformListParser::ParseMatrix()
{
  float m[6];
  uint32_t count;

  if (!ParseArguments(m, ArrayLength(m), &count) || count != 6) {
    return false;
  }

  nsSVGTransform* transform = mTransforms.AppendElement(fallible);
  if (!transform) {
    return false;
  }
  transform->SetMatrix(gfxMatrix(m[0], m[1], m[2], m[3], m[4], m[5]));

  return true;
}
