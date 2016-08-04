/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGFEATURES_H__
#define __NS_SVGFEATURES_H__

#include "nsString.h"

class nsSVGFeatures
{
public:
  /**
   * Check whether we support the given feature string.
   *
   * @param aObject the object, which should support the feature,
   *        for example nsIDOMNode or nsIDOMDOMImplementation
   * @param aFeature one of the feature strings specified at
   *    http://www.w3.org/TR/SVG11/feature.html
   */
  static bool
  HasFeature(nsISupports* aObject, const nsAString& aFeature);

  /**
   * Check whether we support the given extension string.
   *
   * @param aExtension the URI of an extension. Known extensions are
   *   "http://www.w3.org/1999/xhtml" and "http://www.w3.org/1998/Math/MathML"
   */
  static bool
  HasExtension(const nsAString& aExtension, const bool aIsInChrome);
};

#endif // __NS_SVGFEATURES_H__
