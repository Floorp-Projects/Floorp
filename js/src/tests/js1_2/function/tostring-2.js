// |reftest| skip -- obsolete test
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          tostring-1.js
   Section:            Function.toString
   Description:

   Since the behavior of Function.toString() is implementation-dependent,
   toString tests for function are not in the ECMA suite.

   Currently, an attempt to parse the toString output for some functions
   and verify that the result is something reasonable.

   This verifies
   http://scopus.mcom.com/bugsplat/show_bug.cgi?id=99212

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "tostring-2";
var VERSION = "JS1_2";
var TITLE   = "Function.toString()";
var BUGNUMBER="123444";
startTest();

writeHeaderToLog( SECTION + " "+ TITLE);

var tab = "    ";


var equals = new TestFunction( "Equals", "a, b", tab+ "return a == b;" );
function Equals (a, b) {
  return a == b;
}

var reallyequals = new TestFunction( "ReallyEquals", "a, b",
				     ( version() <= 120 )  ? tab +"return a == b;" : tab +"return a === b;" );
function ReallyEquals( a, b ) {
  return a === b;
}

var doesntequal = new TestFunction( "DoesntEqual", "a, b", tab + "return a != b;" );
function DoesntEqual( a, b ) {
  return a != b;
}

var reallydoesntequal = new TestFunction( "ReallyDoesntEqual", "a, b",
					  ( version() <= 120 ) ? tab +"return a != b;"  : tab +"return a !== b;" );
function ReallyDoesntEqual( a, b ) {
  return a !== b;
}

var testor = new TestFunction( "TestOr", "a",  tab+"if (a == null || a == void 0) {\n"+
			       tab +tab+"return 0;\n"+tab+"} else {\n"+tab+tab+"return a;\n"+tab+"}" );
function TestOr( a ) {
  if ( a == null || a == void 0 )
    return 0;
  else
    return a;
}

var testand = new TestFunction( "TestAnd", "a", tab+"if (a != null && a != void 0) {\n"+
				tab+tab+"return a;\n" + tab+ "} else {\n"+tab+tab+"return 0;\n"+tab+"}" );
function TestAnd( a ) {
  if ( a != null && a != void 0 )
    return a;
  else
    return 0;
}

var or = new TestFunction( "Or", "a, b", tab + "return a | b;" );
function Or( a, b ) {
  return a | b;
}

var and = new TestFunction( "And", "a, b", tab + "return a & b;" );
function And( a, b ) {
  return a & b;
}

var xor = new TestFunction( "XOr", "a, b", tab + "return a ^ b;" );
function XOr( a, b ) {
  return a ^ b;
}

new TestCase( SECTION,
	      "Equals.toString()",
	      equals.valueOf(),
	      Equals.toString() );

new TestCase( SECTION,
	      "ReallyEquals.toString()",
	      reallyequals.valueOf(),
	      ReallyEquals.toString() );

new TestCase( SECTION,
	      "DoesntEqual.toString()",
	      doesntequal.valueOf(),
	      DoesntEqual.toString() );

new TestCase( SECTION,
	      "ReallyDoesntEqual.toString()",
	      reallydoesntequal.valueOf(),
	      ReallyDoesntEqual.toString() );

new TestCase( SECTION,
	      "TestOr.toString()",
	      testor.valueOf(),
	      TestOr.toString() );

new TestCase( SECTION,
	      "TestAnd.toString()",
	      testand.valueOf(),
	      TestAnd.toString() );

new TestCase( SECTION,
	      "Or.toString()",
	      or.valueOf(),
	      Or.toString() );

new TestCase( SECTION,
	      "And.toString()",
	      and.valueOf(),
	      And.toString() );

new TestCase( SECTION,
	      "XOr.toString()",
	      xor.valueOf(),
	      XOr.toString() );

test();

function TestFunction( name, args, body ) {
  this.name = name;
  this.arguments = args.toString();
  this.body = body;

  /* the format of Function.toString() in JavaScript 1.2 is:
     function name ( arguments ) {
     body
     }
  */
  this.value = "function " + (name ? name : "anonymous" )+
    "("+args+") {\n"+ (( body ) ? body +"\n" : "") + "}";

  this.toString = new Function( "return this.value" );
  this.valueOf = new Function( "return this.value" );
  return this;
}
