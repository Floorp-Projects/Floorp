/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 406572;
var summary = 'JSOP_CLOSURE unconditionally replaces properties of the variable object - Browser only';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

if (typeof window != 'undefined')
{
  try {
    actual = "FAIL: Unexpected exception thrown";

    var win = window;
    var windowString = String(window);
    window = 1;
    reportCompare(windowString, String(window), "window should be readonly");

    if (1)
      function window() { return 1; }

    // We should reach this line without throwing. Annex B means the
    // block-scoped function above gets an assignment to 'window' in the
    // nearest 'var' environment, but since 'window' is read-only, the
    // assignment silently fails.
    actual = "";

    // The test harness might rely on window having its original value:
    // restore it.
    window = win;
  } catch (e) {
  }
}
else
{
  expect = actual = 'Test can only run in a Gecko 1.9 browser or later.';
  print(actual);
}
reportCompare(expect, actual, summary);


