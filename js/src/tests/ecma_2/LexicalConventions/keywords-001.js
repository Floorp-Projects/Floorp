/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 *  File Name:
 *  ECMA Section:
 *  Description:
 *
 *
 *  Author:             christine@netscape.com
 *  Date:               11 August 1998
 */
var SECTION = "";
var TITLE   = "Keywords";


var result = "failed";

try {
  eval("super;");
}
catch (x) {
  if (x instanceof SyntaxError)
    result = x.name;
}

AddTestCase(
  "using the expression \"super\" shouldn't cause js to crash",
  "SyntaxError",
  result );

test();
