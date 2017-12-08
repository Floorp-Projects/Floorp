/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.6.4.3-2.js
   ECMA Section:       15.6.4.3 Boolean.prototype.valueOf()
   Description:        Returns this boolean value.

   The valueOf function is not generic; it generates
   a runtime error if its this value is not a Boolean
   object.  Therefore it cannot be transferred to other
   kinds of objects for use as a method.

   Author:             christine@netscape.com
   Date:               june 27, 1997
*/

var SECTION = "15.6.4.3-2";
var TITLE   = "Boolean.prototype.valueOf()";

writeHeaderToLog( SECTION + " "+ TITLE);


new TestCase( "valof=Boolean.prototype.valueOf; x=new Boolean(); x.valueOf=valof;x.valueOf()", false, eval("valof=Boolean.prototype.valueOf; x=new Boolean(); x.valueOf=valof;x.valueOf()") );
   
new TestCase( "valof=Boolean.prototype.valueOf; x=new Boolean(true); x.valueOf=valof;x.valueOf()", true, eval("valof=Boolean.prototype.valueOf; x=new Boolean(true); x.valueOf=valof;x.valueOf()") );

test();
