/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          7.7.1.js
   ECMA Section:       7.7.1 Null Literals

   Description:        NullLiteral::
   null


   The value of the null literal null is the sole value
   of the Null type, namely null.

   Author:             christine@netscape.com
   Date:               21 october 1997
*/
var SECTION = "7.7.1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Null Literals";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION, "null",     null,        null);

test();
