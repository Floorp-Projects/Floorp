/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 *  File Name:          toString-001-n.js
 *  Description:
 *
 *  Function.prototype.toString is not generic.
 *
 *  Author:             christine@netscape.com
 *  Date:               11 August 1998
 */
var SECTION = "toString-001.js";
var VERSION = "JS1_4";
var TITLE   = "Regression test case for 310514";
var BUGNUMBER="310514";

startTest();

writeHeaderToLog( SECTION + " "+ TITLE);

var o = {};
o.toString = Function.prototype.toString;

DESCRIPTION = "var o = {}; o.toString = Function.prototype.toString; o.toString();";
EXPECTED = "error";

new TestCase(
  SECTION,
  "var o = {}; o.toString = Function.prototype.toString; o.toString();",
  "error",
  o.toString() );

test();
