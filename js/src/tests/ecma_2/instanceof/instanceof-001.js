/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          instanceof-1.js
   ECMA Section:
   Description:        instanceof operator

   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "";
var VERSION = "ECMA_2";
var TITLE   = "instanceof operator";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

var b = new Boolean();

new TestCase( SECTION,
              "var b = new Boolean(); b instanceof Boolean",
              true,
              b instanceof Boolean );

new TestCase( SECTION,
              "b instanceof Object",
              true,
              b instanceof Object );

test();
