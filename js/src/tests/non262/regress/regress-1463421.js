/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var BUGNUMBER = 1463421;
var summary = 'rogue read barrier';

printBugNumber(BUGNUMBER);
printStatus (summary);

var exc;
try {
var __v_1173 = new WeakMap();
  grayRoot().map = __v_1173;
  __v_1173 = null;
  gczeal(13, 7);
if (!isNaN()) {
}
  (function __f_252() {
      ( {
      })()
  })();
} catch (e) {
  exc = e;
}

if (typeof reportCompare == 'function')
    reportCompare("" + exc, "TypeError: ({}) is not a function", "ok");
