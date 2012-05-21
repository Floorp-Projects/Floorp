/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 *  File Name:   eval-001.js
 *  Original Description: (SEE REVISED DESCRIPTION FURTHER BELOW)
 *
 *  The global eval function may not be accessed indirectly and then called.
 *  This feature will continue to work in JavaScript 1.3 but will result in an
 *  error in JavaScript 1.4. This restriction is also in place for the With and
 *  Closure constructors.
 *
 *  http://scopus.mcom.com/bugsplat/show_bug.cgi?id=324451
 *
 *  Author:          christine@netscape.com
 *  Date:               11 August 1998
 *
 *
 *  REVISION:    05  February 2001
 *  Author:          pschwartau@netscape.com
 * 
 *  Indirect eval IS NOT ILLEGAL per ECMA3!!!  See
 *
 *  http://bugzilla.mozilla.org/show_bug.cgi?id=38512
 *
 *  ------- Additional Comments From Brendan Eich 2001-01-30 17:12 -------
 * ECMA-262 Edition 3 doesn't require implementations to throw EvalError,
 * see the short, section-less Chapter 16.  It does say an implementation that
 * doesn't throw EvalError must allow assignment to eval and indirect calls
 * of the evalnative method.
 *
 */
var SECTION = "eval-001.js";
var VERSION = "JS1_4";
var TITLE   = "Calling eval indirectly should NOT fail in version 140";
var BUGNUMBER="38512";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

var MY_EVAL = eval;
var RESULT = "";
var EXPECT = "abcdefg";

MY_EVAL( "RESULT = EXPECT" );

new TestCase(
  SECTION,
  "Call eval indirectly",
  EXPECT,
  RESULT );

test();

