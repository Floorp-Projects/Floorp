/*
* The contents of this file are subject to the Netscape Public License
* Version 1.0 (the "NPL"); you may not use this file except in
* compliance with the NPL.  You may obtain a copy of the NPL at
* http://www.mozilla.org/NPL/
*
* Software distributed under the NPL is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
* for the specific language governing rights and limitations under the
* NPL.
*
* The Initial Developer of this code under the NPL is Netscape
* Communications Corporation.  Portions created by Netscape are
* Copyright (C) 1998 Netscape Communications Corporation.  All Rights
* Reserved.
*/

/**
 *  File Name:          regress-7635.js
 *  Reference:          http://bugzilla.mozilla.org/show_bug.cgi?id=7635
 *  Description:        instanceof tweaks
 *  Author:             
 */

    var SECTION = "instanceof";       // provide a document reference (ie, ECMA section)
    var VERSION = "ECMA_2"; // Version of JavaScript or ECMA
    var TITLE   = "Regression test for Bugzilla #7635";       // Provide ECMA section title or a description
    var BUGNUMBER = "http://bugzilla.mozilla.org/show_bug.cgi?id=7635";     // Provide URL to bugsplat or bugzilla report

    startTest();               // leave this alone

    /*
     * Calls to AddTestCase here. AddTestCase is a function that is defined
     * in shell.js and takes three arguments:
     * - a string representation of what is being tested
     * - the expected result
     * - the actual result
     *
     * For example, a test might look like this:
     *
     * var zip = /[\d]{5}$/;
     *
     * AddTestCase(
     * "zip = /[\d]{5}$/; \"PO Box 12345 Boston, MA 02134\".match(zip)",   // description of the test
     *  "02134",                                                           // expected result
     *  "PO Box 12345 Boston, MA 02134".match(zip) );                      // actual result
     *
     */

	function Foo() {}
	theproto = {};
	Foo.prototype = theproto
	theproto instanceof Foo


	AddTestCase( "function Foo() {}; theproto = {}; Foo.prototype = theproto; theproto instanceof Foo",
			false,
			theproto instanceof Foo );
	
	var o  = {};

	AddTestCase( "var o = {}; o instanceof o", false, o instanceof o );

	var f = new Function();

	AddTestCase( "var f = new Function(); f instanceof f", false, f instanceof f );


    test();       // leave this alone.  this executes the test cases and
                  // displays results.
