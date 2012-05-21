/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.4.4.1.js
   ECMA Section:       15.4.4.1 Array.prototype.constructor
   Description:        The initial value of Array.prototype.constructor
   is the built-in Array constructor.
   Author:             christine@netscape.com
   Date:               7 october 1997
*/

var SECTION = "15.4.4.1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Array.prototype.constructor";

writeHeaderToLog( SECTION + " "+ TITLE);


new TestCase( SECTION,
	      "Array.prototype.constructor == Array",
	      true,  
	      Array.prototype.constructor == Array);

test();
