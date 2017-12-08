/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          10.1.3.js
   ECMA Section:       10.1.3.js Variable Instantiation
   Description:
   Author:             christine@netscape.com
   Date:               11 september 1997
*/

var SECTION = "10.1.3";
var TITLE   = "Variable instantiation";
var BUGNUMBER = "20256";
printBugNumber(BUGNUMBER);

writeHeaderToLog( SECTION + " "+ TITLE);

   
// overriding a variable or function name with a function should succeed
    
new TestCase("function t() { return \"first\" };" +
	     "function t() { return \"second\" };t() ",
	     "second",
	     eval("function t() { return \"first\" };" +
		  "function t() { return \"second\" };t()"));

   
new TestCase("var t; function t(){}; typeof(t)",
	     "function",
	     eval("var t; function t(){}; typeof(t)"));


// formal parameter tests
    
new TestCase("function t1(a,b) { return b; }; t1( 4 );",
	     void 0,
	     eval("function t1(a,b) { return b; }; t1( 4 );") );
   
new TestCase("function t1(a,b) { return a; }; t1(4);",
	     4,
	     eval("function t1(a,b) { return a; }; t1(4)"));
    
new TestCase("function t1(a,b) { return a; }; t1();",
	     void 0,
	     eval("function t1(a,b) { return a; }; t1()"));
   
new TestCase("function t1(a,b) { return a; }; t1(1,2,4);",
	     1,
	     eval("function t1(a,b) { return a; }; t1(1,2,4)"));
/*
   
new TestCase("function t1(a,a) { return a; }; t1( 4 );",
void 0,
eval("function t1(a,a) { return a; }; t1( 4 )"));
    
new TestCase("function t1(a,a) { return a; }; t1( 1,2 );",
2,
eval("function t1(a,a) { return a; }; t1( 1,2 )"));
*/
// variable declarations
   
new TestCase("function t1(a,b) { return a; }; t1( false, true );",
	     false,
	     eval("function t1(a,b) { return a; }; t1( false, true );"));
   
new TestCase("function t1(a,b) { return b; }; t1( false, true );",
	     true,
	     eval("function t1(a,b) { return b; }; t1( false, true );"));
   
new TestCase("function t1(a,b) { return a+b; }; t1( 4, 2 );",
	     6,
	     eval("function t1(a,b) { return a+b; }; t1( 4, 2 );"));
   
new TestCase("function t1(a,b) { return a+b; }; t1( 4 );",
	     Number.NaN,
	     eval("function t1(a,b) { return a+b; }; t1( 4 );"));

// overriding a function name with a variable should fail
   
new TestCase("function t() { return 'function' };" +
	     "var t = 'variable'; typeof(t)",
	     "string",
	     eval("function t() { return 'function' };" +
		  "var t = 'variable'; typeof(t)"));

// function as a constructor
   
new TestCase("function t1(a,b) { var a = b; return a; } t1(1,3);",
	     3,
	     eval("function t1(a, b){ var a = b; return a;}; t1(1,3)"));
   
new TestCase("function t2(a,b) { this.a = b;  } x  = new t2(1,3); x.a",
	     3,
	     eval("function t2(a,b) { this.a = b; };" +
		  "x = new t2(1,3); x.a"));
   
new TestCase("function t2(a,b) { this.a = a;  } x  = new t2(1,3); x.a",
	     1,
	     eval("function t2(a,b) { this.a = a; };" +
		  "x = new t2(1,3); x.a"));
   
new TestCase("function t2(a,b) { this.a = b; this.b = a; } " +
	     "x = new t2(1,3);x.a;",
	     3,
	     eval("function t2(a,b) { this.a = b; this.b = a; };" +
		  "x = new t2(1,3);x.a;"));
   
new TestCase("function t2(a,b) { this.a = b; this.b = a; }" +
	     "x = new t2(1,3);x.b;",
	     1,
	     eval("function t2(a,b) { this.a = b; this.b = a; };" +
		  "x = new t2(1,3);x.b;") );

test();
