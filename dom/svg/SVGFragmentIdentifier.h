/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SVGFRAGMENTIDENTIFIER_H__
#define MOZILLA_SVGFRAGMENTIDENTIFIER_H__

#include "nsString.h"

namespace mozilla {

namespace dom {
class Document;
class SVGSVGElement;
}  // namespace dom

/**
 * Implements support for parsing SVG fragment identifiers
 * http://www.w3.org/TR/SVG/linking.html#SVGFragmentIdentifiers
 */
class SVGFragmentIdentifier {
  // To prevent the class being instantiated
  SVGFragmentIdentifier() = delete;

 public:
  /**
   * Process the SVG fragment identifier, if there is one.
   * @return true if we found a valid svgView()-style fragment identifier,
   * in which case further processing by the caller can stop. Otherwise return
   * false as we may have an ordinary anchor which needs to be :target matched.
   */
  static bool ProcessFragmentIdentifier(dom::Document* aDocument,
                                        const nsAString& aAnchorName);

 private:
  /**
   * Parse an SVG ViewSpec and set applicable attributes on the root element.
   * @return true if there is a valid ViewSpec
   */
  static bool ProcessSVGViewSpec(const nsAString& aViewSpec,
                                 dom::SVGSVGElement* root);
};

}  // namespace mozilla

#endif  // MOZILLA_SVGFRAGMENTIDENTIFIER_H__
