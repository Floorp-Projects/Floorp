/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SVGTRANSFORMLISTPARSER_H__
#define MOZILLA_SVGTRANSFORMLISTPARSER_H__

#include "nsSVGDataParser.h"
#include "nsTArray.h"

////////////////////////////////////////////////////////////////////////
// SVGTransformListParser: taken from nsSVGPathDataParser, a simple
//  recursive descent parser that builds the transform lists from the
//  transform attributes. The grammar for path data
// can be found in SVG 1.1,  chapter 7.
// http://www.w3.org/TR/SVG11/coords.html#TransformAttribute

class nsIAtom;

namespace mozilla {

class nsSVGTransform;

class SVGTransformListParser : public nsSVGDataParser
{
public:
  const nsTArray<nsSVGTransform>& GetTransformList() const {
    return mTransforms;
  }

private:
  nsTArray<nsSVGTransform> mTransforms;

  // helpers
  virtual nsresult Match();

  nsresult MatchNumberArguments(float *aResult,
                                uint32_t aMaxNum,
                                uint32_t *aParsedNum);

  nsresult MatchTransformList();

  nsresult GetTransformToken(nsIAtom** aKeyatom, bool aAdvancePos);
  nsresult MatchTransforms();

  nsresult MatchTransform();

  bool IsTokenTransformStarter();

  nsresult MatchTranslate();

  nsresult MatchScale();
  nsresult MatchRotate();
  nsresult MatchSkewX();
  nsresult MatchSkewY();
  nsresult MatchMatrix();
};

} // namespace mozilla

#endif // MOZILLA_SVGTRANSFORMLISTPARSER_H__
