/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface CSSValue {

  // UnitTypes
  const unsigned short      CSS_INHERIT                    = 0;
  const unsigned short      CSS_PRIMITIVE_VALUE            = 1;
  const unsigned short      CSS_VALUE_LIST                 = 2;
  const unsigned short      CSS_CUSTOM                     = 3;

           [Throws]
           attribute DOMString        cssText;

  readonly attribute unsigned short   cssValueType;
};
