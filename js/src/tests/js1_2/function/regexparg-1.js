/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          regexparg-1.js
   Description:

   Regression test for
   http://scopus/bugsplat/show_bug.cgi?id=122787
   Passing a regular expression as the first constructor argument fails

   Author:             christine@netscape.com
   Date:               15 June 1998
*/

var SECTION = "JS_1.2";
var VERSION = "JS_1.2";
startTest();
var TITLE   = "The variable statement";

writeHeaderToLog( SECTION + " "+ TITLE);

print("Note: Bug 61911 changed the behavior of typeof regexp in Gecko 1.9.");
print("Prior to Gecko 1.9, typeof regexp returned 'function'.");
print("However in Gecko 1.9 and later, typeof regexp will return 'object'.");

function f(x) {return x;}

x = f(/abc/);

new TestCase( SECTION,
	      "function f(x) {return x;}; f()",
	      void 0,
	      f() );

new TestCase( SECTION,
	      "f(\"hi\")",
	      "hi",
	      f("hi") );

new TestCase( SECTION,
	      "new f(/abc/) +''",
	      "/abc/",
	      new f(/abc/) +"" );

new TestCase( SECTION,
	      "f(/abc/)+'')",
	      "/abc/",
	      f(/abc/) +'');   
       
new TestCase( SECTION,
	      "typeof f(/abc/)",
	      "object",
	      typeof f(/abc/) );

new TestCase( SECTION,
	      "typeof new f(/abc/)",
	      "object",
	      typeof new f(/abc/) );

new TestCase( SECTION,
	      "x = new f(/abc/); x.exec(\"hi\")",
	      null,
	      x.exec("hi") );


// js> x()
test();
