/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          12.5-1.js
   ECMA Section:       The if statement
   Description:

   The production IfStatement : if ( Expression ) Statement else Statement
   is evaluated as follows:

   1.Evaluate Expression.
   2.Call GetValue(Result(1)).
   3.Call ToBoolean(Result(2)).
   4.If Result(3) is false, go to step 7.
   5.Evaluate the first Statement.
   6.Return Result(5).
   7.Evaluate the second Statement.
   8.Return Result(7).

   Author:             christine@netscape.com
   Date:               12 november 1997
*/


var SECTION = "12.5-1";
var TITLE   = "The if statement";

writeHeaderToLog( SECTION + " "+ TITLE);


new TestCase(   "var MYVAR; if ( true ) MYVAR='PASSED'; else MYVAR= 'FAILED';",
		"PASSED",
		eval("var MYVAR; if ( true ) MYVAR='PASSED'; else MYVAR= 'FAILED';") );

new TestCase(  "var MYVAR; if ( false ) MYVAR='FAILED'; else MYVAR= 'PASSED';",
	       "PASSED",
	       eval("var MYVAR; if ( false ) MYVAR='FAILED'; else MYVAR= 'PASSED';") );

new TestCase(   "var MYVAR; if ( new Boolean(true) ) MYVAR='PASSED'; else MYVAR= 'FAILED';",
		"PASSED",
		eval("var MYVAR; if ( new Boolean(true) ) MYVAR='PASSED'; else MYVAR= 'FAILED';") );

new TestCase(  "var MYVAR; if ( new Boolean(false) ) MYVAR='PASSED'; else MYVAR= 'FAILED';",
	       "PASSED",
	       eval("var MYVAR; if ( new Boolean(false) ) MYVAR='PASSED'; else MYVAR= 'FAILED';") );

new TestCase(   "var MYVAR; if ( 1 ) MYVAR='PASSED'; else MYVAR= 'FAILED';",
		"PASSED",
		eval("var MYVAR; if ( 1 ) MYVAR='PASSED'; else MYVAR= 'FAILED';") );

new TestCase(  "var MYVAR; if ( 0 ) MYVAR='FAILED'; else MYVAR= 'PASSED';",
	       "PASSED",
	       eval("var MYVAR; if ( 0 ) MYVAR='FAILED'; else MYVAR= 'PASSED';") );

test();

