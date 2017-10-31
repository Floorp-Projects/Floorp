/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.2.4.2.js
   ECMA Section:       15.2.4.2 Object.prototype.toString()

   Description:        When the toString method is called, the following
   steps are taken:
   1.  Get the [[Class]] property of this object
   2.  Call ToString( Result(1) )
   3.  Compute a string value by concatenating the three
   strings "[object " + Result(2) + "]"
   4.  Return Result(3).

   Author:             christine@netscape.com
   Date:               28 october 1997

*/
var SECTION = "15.2.4.2";
var TITLE   = "Object.prototype.toString()";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( "(new Object()).toString()",    "[object Object]",  (new Object()).toString() );

new TestCase( "myvar = this;  myvar.toString = Object.prototype.toString; myvar.toString()",
	      GLOBAL.replace(/ @ 0x[0-9a-fA-F]+ \(native @ 0x[0-9a-fA-F]+\)/, ''),
	      eval("myvar = this;  myvar.toString = Object.prototype.toString; myvar.toString()")
  );

new TestCase( "myvar = MyObject; myvar.toString = Object.prototype.toString; myvar.toString()",
	      "[object Function]",
	      eval("myvar = MyObject; myvar.toString = Object.prototype.toString; myvar.toString()") );

new TestCase( "myvar = new MyObject( true ); myvar.toString = Object.prototype.toString; myvar.toString()",
	      '[object Object]',
	      eval("myvar = new MyObject( true ); myvar.toString = Object.prototype.toString; myvar.toString()") );

new TestCase( "myvar = new Number(0); myvar.toString = Object.prototype.toString; myvar.toString()",
	      "[object Number]",
	      eval("myvar = new Number(0); myvar.toString = Object.prototype.toString; myvar.toString()") );

new TestCase( "myvar = new String(''); myvar.toString = Object.prototype.toString; myvar.toString()",
	      "[object String]",
	      eval("myvar = new String(''); myvar.toString = Object.prototype.toString; myvar.toString()") );

new TestCase( "myvar = Math; myvar.toString = Object.prototype.toString; myvar.toString()",
	      "[object Math]",
	      eval("myvar = Math; myvar.toString = Object.prototype.toString; myvar.toString()") );

new TestCase( "myvar = new Function(); myvar.toString = Object.prototype.toString; myvar.toString()",
	      "[object Function]",
	      eval("myvar = new Function(); myvar.toString = Object.prototype.toString; myvar.toString()") );

new TestCase( "myvar = new Array(); myvar.toString = Object.prototype.toString; myvar.toString()",
	      "[object Array]",
	      eval("myvar = new Array(); myvar.toString = Object.prototype.toString; myvar.toString()") );

new TestCase( "myvar = new Boolean(); myvar.toString = Object.prototype.toString; myvar.toString()",
	      "[object Boolean]",
	      eval("myvar = new Boolean(); myvar.toString = Object.prototype.toString; myvar.toString()") );

new TestCase( "myvar = new Date(); myvar.toString = Object.prototype.toString; myvar.toString()",
	      "[object Date]",
	      eval("myvar = new Date(); myvar.toString = Object.prototype.toString; myvar.toString()") );

new TestCase( "var MYVAR = new Object( this ); MYVAR.toString()",
	      GLOBAL.replace(/ @ 0x[0-9a-fA-F]+ \(native @ 0x[0-9a-fA-F]+\)/, ''),
	      eval("var MYVAR = new Object( this ); MYVAR.toString()")
  );

new TestCase( "var MYVAR = new Object(); MYVAR.toString()",
	      "[object Object]",
	      eval("var MYVAR = new Object(); MYVAR.toString()") );

new TestCase( "var MYVAR = new Object(void 0); MYVAR.toString()",
	      "[object Object]",
	      eval("var MYVAR = new Object(void 0); MYVAR.toString()") );

new TestCase( "var MYVAR = new Object(null); MYVAR.toString()",
	      "[object Object]",
	      eval("var MYVAR = new Object(null); MYVAR.toString()") );


function MyObject( value ) {
  this.value = new Function( "return this.value" );
  this.toString = new Function ( "return this.value+''");
}

test();
