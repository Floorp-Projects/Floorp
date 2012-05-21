// |reftest| skip -- obsolete test
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          toString_1.js
   ECMA Section:       Object.toString()
   Description:

   This checks the ToString value of Object objects under JavaScript 1.2.

   In JavaScript 1.2, Object.toString()

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "JS1_2";
var VERSION = "JS1_2";
startTest();
var TITLE   = "Object.toString()";

writeHeaderToLog( SECTION + " "+ TITLE);

var o = new Object();

new TestCase( SECTION,
	      "var o = new Object(); o.toString()",
	      "{}",
	      o.toString() );

o = {};

new TestCase( SECTION,
	      "o = {}; o.toString()",
	      "{}",
	      o.toString() );

o = { name:"object", length:0, value:"hello" }

  new TestCase( SECTION,
		"o = { name:\"object\", length:0, value:\"hello\" }; o.toString()",
		true,
		checkObjectToString(o.toString(), ['name:"object"', 'length:0',
						   'value:"hello"']));

o = { name:"object", length:0, value:"hello",
      toString:new Function( "return this.value+''" ) }

  new TestCase( SECTION,
		"o = { name:\"object\", length:0, value:\"hello\", "+
		"toString:new Function( \"return this.value+''\" ) }; o.toString()",
		"hello",
		o.toString() );



test();

/**
 * checkObjectToString
 *
 * In JS1.2, Object.prototype.toString returns a representation of the
 * object's properties as a string. However, the order of the properties
 * in the resulting string is not specified. This function compares the
 * resulting string with an array of strings to make sure that the
 * resulting string is some permutation of the strings in the array.
 */
function checkObjectToString(s, a) {
  var m = /^\{(.*)\}$/(s);
  if (!m)
    return false;	// should begin and end with curly brackets
  var a2 = m[1].split(", ");
  if (a.length != a2.length)
    return false;	// should be same length
  a.sort();
  a2.sort();
  for (var i=0; i < a.length; i++) {
    if (a[i] != a2[i])
      return false;	// should have identical elements
  }
  return true;
}

