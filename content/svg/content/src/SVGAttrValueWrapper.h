/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is Mozilla Japan.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef MOZILLA_SVGATTRVALUEWRAPPER_H__
#define MOZILLA_SVGATTRVALUEWRAPPER_H__

/**
 * Utility wrapper for handling SVG types used inside nsAttrValue so that these
 * types don't need to be exported outside the SVG module.
 */

#include "nsString.h"

class nsSVGAngle;
class nsSVGIntegerPair;
class nsSVGLength2;
class nsSVGNumberPair;
class nsSVGViewBox;

namespace mozilla {
class SVGLengthList;
class SVGNumberList;
class SVGPathData;
class SVGPointList;
class SVGAnimatedPreserveAspectRatio;
class SVGStringList;
class SVGTransformList;
}

namespace mozilla {

class SVGAttrValueWrapper
{
public:
  static void ToString(const nsSVGAngle* aAngle, nsAString& aResult);
  static void ToString(const nsSVGIntegerPair* aIntegerPair,
                       nsAString& aResult);
  static void ToString(const nsSVGLength2* aLength, nsAString& aResult);
  static void ToString(const mozilla::SVGLengthList* aLengthList,
                       nsAString& aResult);
  static void ToString(const mozilla::SVGNumberList* aNumberList,
                       nsAString& aResult);
  static void ToString(const nsSVGNumberPair* aNumberPair, nsAString& aResult);
  static void ToString(const mozilla::SVGPathData* aPathData,
                       nsAString& aResult);
  static void ToString(const mozilla::SVGPointList* aPointList,
                       nsAString& aResult);
  static void ToString(
    const mozilla::SVGAnimatedPreserveAspectRatio* aPreserveAspectRatio,
    nsAString& aResult);
  static void ToString(const mozilla::SVGStringList* aStringList,
                       nsAString& aResult);
  static void ToString(const mozilla::SVGTransformList* aTransformList,
                       nsAString& aResult);
  static void ToString(const nsSVGViewBox* aViewBox, nsAString& aResult);
};

} /* namespace mozilla */

#endif // MOZILLA_SVGATTRVALUEWRAPPER_H__
