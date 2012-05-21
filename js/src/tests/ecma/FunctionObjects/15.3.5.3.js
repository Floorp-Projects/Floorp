/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.3.5.3.js
   ECMA Section:       Function.arguments
   Description:

   The value of the arguments property is normally null if there is no
   outstanding invocation of the function in progress (that is, the
   function has been called but has not yet returned). When a non-internal
   Function object (15.3.2.1) is invoked, its arguments property is
   "dynamically bound" to a newly created object that contains the arguments
   on which it was invoked (see 10.1.6 and 10.1.8). Note that the use of this
   property is discouraged; it is provided principally for compatibility
   with existing old code.

   See sections 10.1.6 and 10.1.8 for more extensive tests.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "15.3.5.3";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Function.arguments";

writeHeaderToLog( SECTION + " "+ TITLE);

var MYFUNCTION = new Function( "return this.arguments" );

new TestCase( SECTION,  "var MYFUNCTION = new Function( 'return this.arguments' ); MYFUNCTION.arguments",   null,   MYFUNCTION.arguments );

test();
