/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */
/**
 *  This file contains functions used by all of the XPConnect tests.
 *  See http://www.mozilla.org/scriptable/tests/ for more information.
 */


/**
 *
 *
 */

var FILE_BUGNUMBERS   = "";
var window;
var PASSED;
var FAILED;

function AddTestCase( s, e, a, n, b, i ) {
    TESTCASES[TESTCASES.length] = new TestCase( s, e, a, b, n, i );
    return TESTCASES[TESTCASES.length];
}
function TestCase( s, e, a, n, b, i ) {
    this.id = ID++;
    this.description = s;
    this.expected = e;
    this.actual = a;
    if ( n )
        this.negative = n;
    if ( b )
        this.bugnumber = b;
    if ( i )
        this.ignore = i;
    this.passed = GetResult( e, a );
}

function StartTest( t ) {
    TESTCASES =  new Array();
    FILE_FAILED_CASES = 0;
    FILE_PASSED_CASES = 0;
    FILE_PASSED       = true;
    COMPLETED         = false;
    ID                = 0;

    FILE_TITLE = t;

    WriteLine("\n" + FILE_TITLE  +"\n");

    if ( window ) {
        document.open();
        PASSED = "<font color=\"#00cc00\">passed </font>";
        FAILED = "<font color=\"#ff0000\">FAILED </font>";

    } else {
        PASSED = "passed ";
        FAILED = "FAILED ";
    }
}
function StopTest() {
    // here, we will close up and print a summary of what happened
    writeReadableResults();
    //writeParseableResults();
    writeTestFileSummary();

    if ( window ) {
        document.close();
    }
}
function AddComment(s) {
    WriteLine(s);
}

function writeReadableResults() {
    for ( var i = 0; i < TESTCASES.length; i++ ) {
        var tc = TESTCASES[i];
        if (tc.passed && this.DONT_PRINT_PASSED_TESTS)
            continue;
        WriteLine(
            (tc.passed ? PASSED : FAILED) +
            tc.description + " = " +
            tc.actual      + " " +
            (tc.passed ? "" : "expected " + tc.expected)
        );
    }
}

function writeParseableResults() {
    WriteLine( "START TEST CASE RESULTS" );
    for ( var i = 0; i < TESTCASES.length; i++ ) {
        var tc = TESTCASES[i];

        WriteLine( tc.id +","+
                   tc.description +","+
                   tc.expected +","+
                   tc.actual +","+
                   tc.bugnumber +","+
                   tc.negative  +","+
                   tc.ignore    +","+
                   tc.exception +","+
                   tc.passed );
    }
}
function writeTestFileSummary() {
    WriteLine ("\nTEST FILE SUMMARY" );

    WriteLine( "Title:        " + FILE_TITLE );
    WriteLine( "Passed:       " + FILE_PASSED );
    WriteLine( "Testcases:    " + TESTCASES.length );
    WriteLine( "Passed Cases: " + FILE_PASSED_CASES );
    WriteLine( "Failed Cases: " + FILE_FAILED_CASES );

    // if we're in the shell, run the garbage collector.
    var gc;
    if ( typeof gc == "function") {
        gc();
    }
}
function GetResult(expect, actual) {
    if ( actual != actual ) {
        if ( typeof actual == "object" ){
            actual = "NaN object";
        } else {
            actual = "NaN number";
        }
    }
    if ( expect != expect ) {
        if ( typeof expect == "object" ) {
            expect = "NaN object";
        } else {
            expect = "NaN number";
        }
    }

    var passed = ( expect == actual ) ? true : false;

    if ( typeof(expect) != typeof(actual) ) {
        passed = false;
    }

    if ( !passed ) {
        FILE_PASSED = false;
        FILE_FAILED_CASES++;
    } else {
        FILE_PASSED_CASES++;
    }


    return passed;
}

function PrintResult(e, a, s, p) {
}

function PrintHTMLFormattedResult( e, a, s, p ) {
}
function WriteLine( s ) {
    if ( window ) {
        document.write( s +"<br>");
    } else {
        print ( s );
    }
}

function GetFailedCases() {
    for ( var i = 0; i < TESTCASES.length; i++ ) {
        var tc = TESTCASES[i];

        if ( !tc.passed )
        WriteLine(
            (tc.passed ? "passed  " : "FAILED! ") +
            tc.description + " = " +
            tc.actual      + " " +
            (tc.passed ? "" : "expected " + tc.expected)
        );
    }
}

/**
 *  Given an object, display all its properties and the value of that
 *  property.
 */
function Enumerate( o ) {
    var p;
    WriteLine( "Properties of object " + o );
    for ( p in o ) {
        WriteLine( p +": "+ (typeof o[p] == "function" ? "function" : o[p]) );
    }
}
/**
 *  These are variables whose values depend on the host environment.
 *  The defaults here are correct for the JavaScript or XPConnect shell.
 *  In order to run the tests in the browser, need to override these
 *  values for the tests to execute correctly.
 *
 */

var GLOBAL = "[object global]";

