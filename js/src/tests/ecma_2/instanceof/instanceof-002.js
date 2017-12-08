/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:
   ECMA Section:
   Description:        Call Objects



   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "";
var TITLE   = "The Call Constructor";

writeHeaderToLog( SECTION + " "+ TITLE);

var b = new Boolean();

new TestCase( "var b = new Boolean(); b instanceof Boolean",
	      true,
	      b instanceof Boolean );

new TestCase( "b instanceof Object",
	      true,
	      b instanceof Object );

new TestCase( "b instanceof Array",
	      false,
	      b instanceof Array );

new TestCase( "true instanceof Boolean",
	      false,
	      true instanceof Boolean );

new TestCase( "Boolean instanceof Object",
	      true,
	      Boolean instanceof Object );
test();

