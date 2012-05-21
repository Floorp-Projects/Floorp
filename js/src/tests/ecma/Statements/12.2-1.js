/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          12.2-1.js
   ECMA Section:       The variable statement
   Description:

   If the variable statement occurs inside a FunctionDeclaration, the
   variables are defined with function-local scope in that function, as
   described in section 10.1.3. Otherwise, they are defined with global
   scope, that is, they are created as members of the global object, as
   described in section 0. Variables are created when the execution scope
   is entered. A Block does not define a new execution scope. Only Program and
   FunctionDeclaration produce a new scope. Variables are initialized to the
   undefined value when created. A variable with an Initializer is assigned
   the value of its AssignmentExpression when the VariableStatement is executed,
   not when the variable is created.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "12.2-1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "The variable statement";

writeHeaderToLog( SECTION +" "+ TITLE);

new TestCase(    "SECTION",
		 "var x = 3; function f() { var a = x; var x = 23; return a; }; f()",
		 void 0,
		 eval("var x = 3; function f() { var a = x; var x = 23; return a; }; f()") );

test();

