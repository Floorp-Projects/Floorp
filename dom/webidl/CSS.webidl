/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dev.w3.org/csswg/css3-conditional/
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

interface CSS {
  [Throws]
  static boolean supports(DOMString property, DOMString value);

  [Throws]
  static boolean supports(DOMString conditionText);
};

// http://dev.w3.org/csswg/cssom/#the-css.escape%28%29-method
partial interface CSS {
  [Throws]
  static DOMString escape(DOMString ident);
};
