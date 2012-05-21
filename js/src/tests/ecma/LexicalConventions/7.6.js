/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          7.6.js
   ECMA Section:       Punctuators
   Description:

   This tests verifies that all ECMA punctutors are recognized as a
   token separator, but does not attempt to verify the functionality
   of any punctuator.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "7.6";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Punctuators";

writeHeaderToLog( SECTION + " "+ TITLE);

// ==
new TestCase( SECTION,
	      "var c,d;c==d",
	      true,
	      eval("var c,d;c==d") );

// =

new TestCase( SECTION,
	      "var a=true;a",
	      true,
	      eval("var a=true;a") );

// >
new TestCase( SECTION,
	      "var a=true,b=false;a>b",
	      true,
	      eval("var a=true,b=false;a>b") );

// <
new TestCase( SECTION,
	      "var a=true,b=false;a<b",
	      false,
	      eval("var a=true,b=false;a<b") );

// <=
new TestCase( SECTION,
	      "var a=0xFFFF,b=0X0FFF;a<=b",
	      false,
	      eval("var a=0xFFFF,b=0X0FFF;a<=b") );

// >=
new TestCase( SECTION,
	      "var a=0xFFFF,b=0XFFFE;a>=b",
	      true,
	      eval("var a=0xFFFF,b=0XFFFE;a>=b") );

// !=
new TestCase( SECTION,
	      "var a=true,b=false;a!=b",
	      true,
	      eval("var a=true,b=false;a!=b") );

new TestCase( SECTION,
	      "var a=false,b=false;a!=b",
	      false,
	      eval("var a=false,b=false;a!=b") );
// ,
new TestCase( SECTION,
	      "var a=true,b=false;a,b",
	      false,
	      eval("var a=true,b=false;a,b") );
// !
new TestCase( SECTION,
	      "var a=true,b=false;!a",
	      false,
	      eval("var a=true,b=false;!a") );

// ~
new TestCase( SECTION,
	      "var a=true;~a",
	      -2,
	      eval("var a=true;~a") );
// ?
new TestCase( SECTION,
	      "var a=true; (a ? 'PASS' : '')",
	      "PASS",
	      eval("var a=true; (a ? 'PASS' : '')") );

// :

new TestCase( SECTION,
	      "var a=false; (a ? 'FAIL' : 'PASS')",
	      "PASS",
	      eval("var a=false; (a ? 'FAIL' : 'PASS')") );
// .

new TestCase( SECTION,
	      "var a=Number;a.NaN",
	      NaN,
	      eval("var a=Number;a.NaN") );

// &&
new TestCase( SECTION,
	      "var a=true,b=true;if(a&&b)'PASS';else'FAIL'",
	      "PASS",
	      eval("var a=true,b=true;if(a&&b)'PASS';else'FAIL'") );

// ||
new TestCase( SECTION,
	      "var a=false,b=false;if(a||b)'FAIL';else'PASS'",
	      "PASS",
	      eval("var a=false,b=false;if(a||b)'FAIL';else'PASS'") );
// ++
new TestCase( SECTION,
	      "var a=false,b=false;++a",
	      1,
	      eval("var a=false,b=false;++a") );
// --
new TestCase( SECTION,
	      "var a=true,b=false--a",
	      0,
	      eval("var a=true,b=false;--a") );
// +

new TestCase( SECTION,
	      "var a=true,b=true;a+b",
	      2,
	      eval("var a=true,b=true;a+b") );
// -
new TestCase( SECTION,
	      "var a=true,b=true;a-b",
	      0,
	      eval("var a=true,b=true;a-b") );
// *
new TestCase( SECTION,
	      "var a=true,b=true;a*b",
	      1,
	      eval("var a=true,b=true;a*b") );
// /
new TestCase( SECTION,
	      "var a=true,b=true;a/b",
	      1,
	      eval("var a=true,b=true;a/b") );
// &
new TestCase( SECTION,
	      "var a=3,b=2;a&b",
	      2,
	      eval("var a=3,b=2;a&b") );
// |
new TestCase( SECTION,
	      "var a=4,b=3;a|b",
	      7,
	      eval("var a=4,b=3;a|b") );

// |
new TestCase( SECTION,
	      "var a=4,b=3;a^b",
	      7,
	      eval("var a=4,b=3;a^b") );

// %
new TestCase( SECTION,
	      "var a=4,b=3;a|b",
	      1,
	      eval("var a=4,b=3;a%b") );

// <<
new TestCase( SECTION,
	      "var a=4,b=3;a<<b",
	      32,
	      eval("var a=4,b=3;a<<b") );

//  >>
new TestCase( SECTION,
	      "var a=4,b=1;a>>b",
	      2,
	      eval("var a=4,b=1;a>>b") );

//  >>>
new TestCase( SECTION,
	      "var a=1,b=1;a>>>b",
	      0,
	      eval("var a=1,b=1;a>>>b") );
//  +=
new TestCase( SECTION,
	      "var a=4,b=3;a+=b;a",
	      7,
	      eval("var a=4,b=3;a+=b;a") );

//  -=
new TestCase( SECTION,
	      "var a=4,b=3;a-=b;a",
	      1,
	      eval("var a=4,b=3;a-=b;a") );
//  *=
new TestCase( SECTION,
	      "var a=4,b=3;a*=b;a",
	      12,
	      eval("var a=4,b=3;a*=b;a") );
//  +=
new TestCase( SECTION,
	      "var a=4,b=3;a+=b;a",
	      7,
	      eval("var a=4,b=3;a+=b;a") );
//  /=
new TestCase( SECTION,
	      "var a=12,b=3;a/=b;a",
	      4,
	      eval("var a=12,b=3;a/=b;a") );

//  &=
new TestCase( SECTION,
	      "var a=4,b=5;a&=b;a",
	      4,
	      eval("var a=4,b=5;a&=b;a") );

// |=
new TestCase( SECTION,
	      "var a=4,b=5;a&=b;a",
	      5,
	      eval("var a=4,b=5;a|=b;a") );
//  ^=
new TestCase( SECTION,
	      "var a=4,b=5;a^=b;a",
	      1,
	      eval("var a=4,b=5;a^=b;a") );
// %=
new TestCase( SECTION,
	      "var a=12,b=5;a%=b;a",
	      2,
	      eval("var a=12,b=5;a%=b;a") );
// <<=
new TestCase( SECTION,
	      "var a=4,b=3;a<<=b;a",
	      32,
	      eval("var a=4,b=3;a<<=b;a") );

//  >>
new TestCase( SECTION,
	      "var a=4,b=1;a>>=b;a",
	      2,
	      eval("var a=4,b=1;a>>=b;a") );

//  >>>
new TestCase( SECTION,
	      "var a=1,b=1;a>>>=b;a",
	      0,
	      eval("var a=1,b=1;a>>>=b;a") );

// ()
new TestCase( SECTION,
	      "var a=4,b=3;(a)",
	      4,
	      eval("var a=4,b=3;(a)") );
// {}
new TestCase( SECTION,
	      "var a=4,b=3;{b}",
	      3,
	      eval("var a=4,b=3;{b}") );

// []
new TestCase( SECTION,
	      "var a=new Array('hi');a[0]",
	      "hi",
	      eval("var a=new Array('hi');a[0]") );
// []
new TestCase( SECTION,
	      ";",
	      void 0,
	      eval(";") );
test();

