/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          11.2.1-4-n.js
   ECMA Section:       11.2.1 Property Accessors
   Description:

   Properties are accessed by name, using either the dot notation:
   MemberExpression . Identifier
   CallExpression . Identifier

   or the bracket notation:    MemberExpression [ Expression ]
   CallExpression [ Expression ]

   The dot notation is explained by the following syntactic conversion:
   MemberExpression . Identifier
   is identical in its behavior to
   MemberExpression [ <identifier-string> ]
   and similarly
   CallExpression . Identifier
   is identical in its behavior to
   CallExpression [ <identifier-string> ]
   where <identifier-string> is a string literal containing the same sequence
   of characters as the Identifier.

   The production MemberExpression : MemberExpression [ Expression ] is
   evaluated as follows:

   1.  Evaluate MemberExpression.
   2.  Call GetValue(Result(1)).
   3.  Evaluate Expression.
   4.  Call GetValue(Result(3)).
   5.  Call ToObject(Result(2)).
   6.  Call ToString(Result(4)).
   7.  Return a value of type Reference whose base object is Result(5) and
   whose property name is Result(6).

   The production CallExpression : CallExpression [ Expression ] is evaluated
   in exactly the same manner, except that the contained CallExpression is
   evaluated in step 1.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "11.2.1-4-n";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Property Accessors";
writeHeaderToLog( SECTION + " "+TITLE );

// go through all Native Function objects, methods, and properties and get their typeof.

var PROPERTY = new Array();
var p = 0;

// try to access properties of primitive types

PROPERTY[p++] = new Property(  "null",    null,   "null",   0 );

for ( var i = 0, RESULT; i < PROPERTY.length; i++ ) {

  DESCRIPTION = PROPERTY[i].object + ".valueOf()";
  EXPECTED = "error";

  new TestCase( SECTION,
                PROPERTY[i].object + ".valueOf()",
                PROPERTY[i].value,
                eval( PROPERTY[i].object+ ".valueOf()" ) );

  new TestCase( SECTION,
                PROPERTY[i].object + ".toString()",
                PROPERTY[i].string,
                eval(PROPERTY[i].object+ ".toString()") );

}

test();

function MyObject( value ) {
  this.value = value;
  this.stringValue = value +"";
  this.numberValue = Number(value);
  return this;
}
function Property( object, value, string, number ) {
  this.object = object;
  this.string = String(value);
  this.number = Number(value);
  this.value = value;
}
