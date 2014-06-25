/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.7.3.1-4.js
   ECMA Section:       15.7.3.1 Number.prototype
   Description:        All value properties of the Number object should have
   the attributes [DontEnum, DontDelete, ReadOnly]

   this test checks the DontEnum attribute of Number.prototype

   Author:             christine@netscape.com
   Date:               16 september 1997
*/

var VERSION = "ECMA_1";
startTest();
var SECTION = "15.7.3.1-3";
var TITLE   = "Number.prototype";

writeHeaderToLog( SECTION + " Number.prototype:  DontEnum Attribute");

new TestCase(
  SECTION,
  "var string = ''; for ( prop in Number ) { string += ( prop == 'prototype' ) ? prop: '' } string;",
  "",
  eval("var string = ''; for ( prop in Number ) { string += ( prop == 'prototype' ) ? prop : '' } string;")
  );

test();
