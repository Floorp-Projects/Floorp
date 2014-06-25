/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          10.1.4-9.js
   ECMA Section:       10.1.4 Scope Chain and Identifier Resolution
   Description:
   Every execution context has associated with it a scope chain. This is
   logically a list of objects that are searched when binding an Identifier.
   When control enters an execution context, the scope chain is created and
   is populated with an initial set of objects, depending on the type of
   code. When control leaves the execution context, the scope chain is
   destroyed.

   During execution, the scope chain of the execution context is affected
   only by WithStatement. When execution enters a with block, the object
   specified in the with statement is added to the front of the scope chain.
   When execution leaves a with block, whether normally or via a break or
   continue statement, the object is removed from the scope chain. The object
   being removed will always be the first object in the scope chain.

   During execution, the syntactic production PrimaryExpression : Identifier
   is evaluated using the following algorithm:

   1.  Get the next object in the scope chain. If there isn't one, go to step 5.
   2.  Call the [[HasProperty]] method of Result(l), passing the Identifier as
   the property.
   3.  If Result(2) is true, return a value of type Reference whose base object
   is Result(l) and whose property name is the Identifier.
   4.  Go to step 1.
   5.  Return a value of type Reference whose base object is null and whose
   property name is the Identifier.
   The result of binding an identifier is always a value of type Reference with
   its member name component equal to the identifier string.
   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "10.1.4-9";
var VERSION = "ECMA_2";
startTest();

writeHeaderToLog( SECTION + " Scope Chain and Identifier Resolution");

new TestCase( SECTION, "NEW_PROPERTY =  " );

test();

function test() {
  for ( gTc=0; gTc < gTestcases.length; gTc++ ) {

    var MYOBJECT = new MyObject();
    var RESULT   = "hello";

    with ( MYOBJECT ) {
      NEW_PROPERTY = RESULT;
    }
    gTestcases[gTc].actual = NEW_PROPERTY;
    gTestcases[gTc].expect = RESULT;

    gTestcases[gTc].passed = writeTestCaseResult(
      gTestcases[gTc].expect,
      gTestcases[gTc].actual,
      gTestcases[gTc].description +" = "+
      gTestcases[gTc].actual );

    gTestcases[gTc].reason += ( gTestcases[gTc].passed ) ? "" : "wrong value ";
  }
  stopTest();
  return ( gTestcases );
}
function MyObject( n ) {
  this.__proto__ = Number.prototype;
}
