/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */



/**
 *  File Name:          regress-9141.js
 *  Reference:          "http://bugzilla.mozilla.org/show_bug.cgi?id=9141";
 *  Description:
 *  From waldemar@netscape.com:
 *
 * The following page crashes the system:
 *
 * <!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0 Transitional//EN"
 * "http://www.w3.org/TR/REC-html40/loose.dtd">
 * <HTML>
 * <HEAD>
 * </HEAD>
 * <BODY>
 * <SCRIPT type="text/javascript">
 * var s = "x";
 *  for (var i = 0; i != 13; i++) s += s;
 * var a = /(?:xx|x)*[slash](s);
 * var b = /(xx|x)*[slash](s);
 * document.write("Results = " + a.length + "," + b.length);
 * </SCRIPT>
 * </BODY>
 */

var SECTION = "js1_2";       // provide a document reference (ie, ECMA section)
var TITLE   = "Regression test for bugzilla # 9141";       // Provide ECMA section title or a description
var BUGNUMBER = "http://bugzilla.mozilla.org/show_bug.cgi?id=9141";     // Provide URL to bugsplat or bugzilla report

printBugNumber(BUGNUMBER);

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

var s = "x";
for (var i = 0; i != 13; i++) s += s;
var a = /(?:xx|x)*/.exec(s);
var b = /(xx|x)*/.exec(s);

AddTestCase( "var s = 'x'; for (var i = 0; i != 13; i++) s += s; " +
	     "a = /(?:xx|x)*/.exec(s); a.length",
	     1,
	     a.length );

AddTestCase( "var b = /(xx|x)*/.exec(s); b.length",
	     2,
	     b.length );

test();       // leave this alone.  this executes the test cases and
// displays results.
