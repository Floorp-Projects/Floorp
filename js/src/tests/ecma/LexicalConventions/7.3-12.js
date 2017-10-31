/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          7.3-12.js
   ECMA Section:       7.3 Comments
   Description:


   Author:             christine@netscape.com
   Date:               12 november 1997

*/
var SECTION = "7.3-12";
var TITLE   = "Comments";

writeHeaderToLog( SECTION + " "+ TITLE);

var actual = "pass";

/*actual="fail";**/

new TestCase( "code following multiline comment",
			     "pass",
			     actual);

test();
