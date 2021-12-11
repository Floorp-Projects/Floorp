/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 483749;
var summary = 'Do not assert: !js_IsActiveWithOrBlock(cx, fp->scopeChain, 0)';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);


for (let x of ['']) {
  for (var b = 0; b < 5; ++b) {
    if (b % 5 == 3) {
      with([]) this;
    }
  }
}


reportCompare(expect, actual, summary);
