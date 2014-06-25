/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.4.2.1-2.js
   ECMA Section:       15.4.2.1 new Array( item0, item1, ... )
   Description:        This description only applies of the constructor is
   given two or more arguments.

   The [[Prototype]] property of the newly constructed
   object is set to the original Array prototype object,
   the one that is the initial value of Array.prototype
   (15.4.3.1).

   The [[Class]] property of the newly constructed object
   is set to "Array".

   The length property of the newly constructed object is
   set to the number of arguments.

   The 0 property of the newly constructed object is set
   to item0... in general, for as many arguments as there
   are, the k property of the newly constructed object is
   set to argument k, where the first argument is
   considered to be argument number 0.


   Author:             christine@netscape.com
   Date:               7 october 1997
*/
var SECTION = "15.4.2.1-2";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "The Array Constructor:  new Array( item0, item1, ...)";

writeHeaderToLog( SECTION + " "+ TITLE);


var TEST_STRING = "new Array(";
var ARGUMENTS = ""
  var TEST_LENGTH = Math.pow(2,10); //Math.pow(2,32);

for ( var index = 0; index < TEST_LENGTH; index++ ) {
  ARGUMENTS += index;
  ARGUMENTS += (index == (TEST_LENGTH-1) ) ? "" : ",";
}

TEST_STRING += ARGUMENTS + ")";

TEST_ARRAY = eval( TEST_STRING );

for ( var item = 0; item < TEST_LENGTH; item++ ) {
  new TestCase( SECTION,
		"["+item+"]",    
		item,   
		TEST_ARRAY[item] );
}

new TestCase( SECTION,
	      "new Array( ["+TEST_LENGTH+" arguments] ) +''",   
	      ARGUMENTS,
	      TEST_ARRAY +"" );

test();
