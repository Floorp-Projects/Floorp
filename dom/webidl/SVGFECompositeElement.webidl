/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.w3.org/TR/SVG2/
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

interface SVGAnimatedEnumeration;
interface SVGAnimatedNumber;
interface SVGAnimatedString;

interface SVGFECompositeElement : SVGElement {

  // Composite Operators
  const unsigned short SVG_FECOMPOSITE_OPERATOR_UNKNOWN = 0;
  const unsigned short SVG_FECOMPOSITE_OPERATOR_OVER = 1;
  const unsigned short SVG_FECOMPOSITE_OPERATOR_IN = 2;
  const unsigned short SVG_FECOMPOSITE_OPERATOR_OUT = 3;
  const unsigned short SVG_FECOMPOSITE_OPERATOR_ATOP = 4;
  const unsigned short SVG_FECOMPOSITE_OPERATOR_XOR = 5;
  const unsigned short SVG_FECOMPOSITE_OPERATOR_ARITHMETIC = 6;

  readonly attribute SVGAnimatedString in1;
  readonly attribute SVGAnimatedString in2;
  readonly attribute SVGAnimatedEnumeration operator;
  readonly attribute SVGAnimatedNumber k1;
  readonly attribute SVGAnimatedNumber k2;
  readonly attribute SVGAnimatedNumber k3;
  readonly attribute SVGAnimatedNumber k4;
};

SVGFECompositeElement implements SVGFilterPrimitiveStandardAttributes;
