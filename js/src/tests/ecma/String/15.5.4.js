/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.5.4.js
   ECMA Section:       15.5.4 Properties of the String prototype object

   Description:
   Author:             christine@netscape.com
   Date:               28 october 1997

*/
var SECTION = "15.5.4";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Properties of the String Prototype objecta";

writeHeaderToLog( SECTION + " "+ TITLE);


new TestCase( SECTION,
	      "String.prototype.getClass = Object.prototype.toString; String.prototype.getClass()",
	      "[object String]",
	      eval("String.prototype.getClass = Object.prototype.toString; String.prototype.getClass()") );

delete String.prototype.getClass;

new TestCase( SECTION,
              "typeof String.prototype",  
              "object",  
              typeof String.prototype );

new TestCase( SECTION,
              "String.prototype.valueOf()",
              "",       
              String.prototype.valueOf() );

new TestCase( SECTION,
              "String.prototype +''",      
              "",       
              String.prototype + '' );

new TestCase( SECTION,
              "String.prototype.length",   
              0,        
              String.prototype.length );

var prop;
var value;

value = '';
for (prop in "")
{
  value += prop;
}
new TestCase( SECTION,
              'String "" has no enumerable properties',
              '',
              value );

value = '';
for (prop in String.prototype)
{
  value += prop;
}
new TestCase( SECTION,
              'String.prototype has no enumerable properties',
              '',
              value );

test();
