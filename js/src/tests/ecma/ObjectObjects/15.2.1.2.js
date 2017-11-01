/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.2.1.2.js
   ECMA Section:       15.2.1.2  The Object Constructor Called as a Function:
   Object(value)
   Description:        When Object is called as a function rather than as a
   constructor, the following steps are taken:

   1.  If value is null or undefined, create and return a
   new object with no proerties other than internal
   properties exactly as if the object constructor
   had been called on that same value (15.2.2.1).
   2.  Return ToObject (value), whose rules are:

   undefined   generate a runtime error
   null        generate a runtime error
   boolean     create a new Boolean object whose default
   value is the value of the boolean.
   number      Create a new Number object whose default
   value is the value of the number.
   string      Create a new String object whose default
   value is the value of the string.
   object      Return the input argument (no conversion).

   Author:             christine@netscape.com
   Date:               17 july 1997
*/

var SECTION = "15.2.1.2";
var TITLE   = "Object()";

writeHeaderToLog( SECTION + " "+ TITLE);

var MYOB = Object();

new TestCase( "var MYOB = Object(); MYOB.valueOf()",    MYOB,      MYOB.valueOf()      );
new TestCase( "typeof Object()",       "object",               typeof (Object(null)) );
new TestCase( "var MYOB = Object(); MYOB.toString()",    "[object Object]",       eval("var MYOB = Object(); MYOB.toString()") );

test();
