/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.5.4.5-4.js
   ECMA Section:       15.5.4.5 String.prototype.charCodeAt(pos)

   Description:        Returns a nonnegative integer less than 2^16.

   Author:             christine@netscape.com
   Date:               28 october 1997

*/
var VERSION = "0697";
startTest();
var SECTION = "15.5.4.5-4";

writeHeaderToLog( SECTION + " String.prototype.charCodeAt(pos)" );

var MAXCHARCODE = Math.pow(2,16);
var item=0, CHARCODE;

for ( CHARCODE=0; CHARCODE <256; CHARCODE++ ) {
  new TestCase( SECTION,
		"(String.fromCharCode("+CHARCODE+")).charCodeAt(0)",
		CHARCODE,
		(String.fromCharCode(CHARCODE)).charCodeAt(0) );
}
for ( CHARCODE=256; CHARCODE < 65536; CHARCODE+=999 ) {
  new TestCase( SECTION,
		"(String.fromCharCode("+CHARCODE+")).charCodeAt(0)",
		CHARCODE,
		(String.fromCharCode(CHARCODE)).charCodeAt(0) );
}

new TestCase( SECTION, "(String.fromCharCode(65535)).charCodeAt(0)", 65535,     (String.fromCharCode(65535)).charCodeAt(0) );

test();
