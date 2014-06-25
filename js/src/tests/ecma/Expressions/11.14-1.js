/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          11.14-1.js
   ECMA Section:       11.14 Comma operator (,)
   Description:
   Expression :

   AssignmentExpression
   Expression , AssignmentExpression

   Semantics

   The production Expression : Expression , AssignmentExpression is evaluated as follows:

   1.  Evaluate Expression.
   2.  Call GetValue(Result(1)).
   3.  Evaluate AssignmentExpression.
   4.  Call GetValue(Result(3)).
   5.  Return Result(4).

   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "11.14-1";
var VERSION = "ECMA_1";
startTest();

writeHeaderToLog( SECTION + " Comma operator (,)");

new TestCase( SECTION,    "true, false",                    false,  eval("true, false") );
new TestCase( SECTION,    "VAR1=true, VAR2=false",          false,  eval("VAR1=true, VAR2=false") );
new TestCase( SECTION,    "VAR1=true, VAR2=false;VAR1",     true,   eval("VAR1=true, VAR2=false; VAR1") );

test();
