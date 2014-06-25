/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          in-001.js
   Section:
   Description:

   http://scopus.mcom.com/bugsplat/show_bug.cgi?id=196109


   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "in-001";
var VERSION = "JS1_3";
var TITLE   = "Regression test for 196109";
var BUGNUMBER="196109";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

o = {};
o.foo = 'sil';

new TestCase(
  SECTION,
  "\"foo\" in o",
  true,
  "foo" in o );

test();
