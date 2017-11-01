/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.5.4.4-3.js
   ECMA Section:       15.5.4.4 String.prototype.charAt(pos)
   Description:        Returns a string containing the character at position
   pos in the string.  If there is no character at that
   string, the result is the empty string.  The result is
   a string value, not a String object.

   When the charAt method is called with one argument,
   pos, the following steps are taken:
   1. Call ToString, with this value as its argument
   2. Call ToInteger pos
   3. Compute the number of characters  in Result(1)
   4. If Result(2) is less than 0 is or not less than
   Result(3), return the empty string
   5. Return a string of length 1 containing one character
   from result (1), the character at position Result(2).

   Note that the charAt function is intentionally generic;
   it does not require that its this value be a String
   object.  Therefore it can be transferred to other kinds
   of objects for use as a method.

   This tests assiging charAt to a user-defined function.

   Author:             christine@netscape.com
   Date:               2 october 1997
*/
var SECTION = "15.5.4.4-3";
var TITLE   = "String.prototype.charAt";

writeHeaderToLog( SECTION + " "+ TITLE);

var foo = new MyObject('hello');


new TestCase( "var foo = new MyObject('hello'); ", "h", foo.charAt(0)  );
new TestCase( "var foo = new MyObject('hello'); ", "e", foo.charAt(1)  );
new TestCase( "var foo = new MyObject('hello'); ", "l", foo.charAt(2)  );
new TestCase( "var foo = new MyObject('hello'); ", "l", foo.charAt(3)  );
new TestCase( "var foo = new MyObject('hello'); ", "o", foo.charAt(4)  );
new TestCase( "var foo = new MyObject('hello'); ", "",  foo.charAt(-1)  );
new TestCase( "var foo = new MyObject('hello'); ", "",  foo.charAt(5)  );

var boo = new MyObject(true);

new TestCase( "var boo = new MyObject(true); ", "t", boo.charAt(0)  );
new TestCase( "var boo = new MyObject(true); ", "r", boo.charAt(1)  );
new TestCase( "var boo = new MyObject(true); ", "u", boo.charAt(2)  );
new TestCase( "var boo = new MyObject(true); ", "e", boo.charAt(3)  );

var noo = new MyObject( Math.PI );

new TestCase( "var noo = new MyObject(Math.PI); ", "3", noo.charAt(0)  );
new TestCase( "var noo = new MyObject(Math.PI); ", ".", noo.charAt(1)  );
new TestCase( "var noo = new MyObject(Math.PI); ", "1", noo.charAt(2)  );
new TestCase( "var noo = new MyObject(Math.PI); ", "4", noo.charAt(3)  );
new TestCase( "var noo = new MyObject(Math.PI); ", "1", noo.charAt(4)  );
new TestCase( "var noo = new MyObject(Math.PI); ", "5", noo.charAt(5)  );
new TestCase( "var noo = new MyObject(Math.PI); ", "9", noo.charAt(6)  );

test();

function MyObject (v) {
  this.value      = v;
  this.toString   = new Function( "return this.value +'';" );
  this.valueOf    = new Function( "return this.value" );
  this.charAt     = String.prototype.charAt;
}

