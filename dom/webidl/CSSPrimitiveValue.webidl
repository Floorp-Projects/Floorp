/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface Counter;
interface Rect;

interface CSSPrimitiveValue : CSSValue {

  // UnitTypes
  const unsigned short      CSS_UNKNOWN                    = 0;
  const unsigned short      CSS_NUMBER                     = 1;
  const unsigned short      CSS_PERCENTAGE                 = 2;
  const unsigned short      CSS_EMS                        = 3;
  const unsigned short      CSS_EXS                        = 4;
  const unsigned short      CSS_PX                         = 5;
  const unsigned short      CSS_CM                         = 6;
  const unsigned short      CSS_MM                         = 7;
  const unsigned short      CSS_IN                         = 8;
  const unsigned short      CSS_PT                         = 9;
  const unsigned short      CSS_PC                         = 10;
  const unsigned short      CSS_DEG                        = 11;
  const unsigned short      CSS_RAD                        = 12;
  const unsigned short      CSS_GRAD                       = 13;
  const unsigned short      CSS_MS                         = 14;
  const unsigned short      CSS_S                          = 15;
  const unsigned short      CSS_HZ                         = 16;
  const unsigned short      CSS_KHZ                        = 17;
  const unsigned short      CSS_DIMENSION                  = 18;
  const unsigned short      CSS_STRING                     = 19;
  const unsigned short      CSS_URI                        = 20;
  const unsigned short      CSS_IDENT                      = 21;
  const unsigned short      CSS_ATTR                       = 22;
  const unsigned short      CSS_COUNTER                    = 23;
  const unsigned short      CSS_RECT                       = 24;
  const unsigned short      CSS_RGBCOLOR                   = 25;

  readonly attribute unsigned short   primitiveType;
  [Throws]
  void               setFloatValue(unsigned short unitType,
                                   float floatValue);
  [Throws]
  float              getFloatValue(unsigned short unitType);
  [Throws]
  void               setStringValue(unsigned short stringType,
                                    DOMString stringValue);
  [Throws]
  DOMString          getStringValue();
  [Throws]
  Counter            getCounterValue();
  [Throws]
  Rect               getRectValue();
  [Throws]
  RGBColor           getRGBColorValue();
};
