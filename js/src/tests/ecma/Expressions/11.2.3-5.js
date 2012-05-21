/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          11.2.3-5-n.js
   ECMA Section:       11.2.3. Function Calls
   Description:

   The production CallExpression : MemberExpression Arguments is evaluated as
   follows:

   1.  Evaluate MemberExpression.
   2.  Evaluate Arguments, producing an internal list of argument values
   (section 0).
   3.  Call GetValue(Result(1)).
   4.  If Type(Result(3)) is not Object, generate a runtime error.
   5.  If Result(3) does not implement the internal [[Call]] method, generate a
   runtime error.
   6.  If Type(Result(1)) is Reference, Result(6) is GetBase(Result(1)). Otherwise,
   Result(6) is null.
   7.  If Result(6) is an activation object, Result(7) is null. Otherwise, Result(7) is
   the same as Result(6).
   8.  Call the [[Call]] method on Result(3), providing Result(7) as the this value
   and providing the list Result(2) as the argument values.
   9.  Return Result(8).

   The production CallExpression : CallExpression Arguments is evaluated in
   exactly the same manner, except that the contained CallExpression is
   evaluated in step 1.

   Note: Result(8) will never be of type Reference if Result(3) is a native
   ECMAScript object. Whether calling a host object can return a value of
   type Reference is implementation-dependent.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "11.2.3-5";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Function Calls";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION, "true.valueOf()", true, true.valueOf() );
test();

