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


/*
 * JavaScript shared functions file for running the tests in either
 * stand-alone JavaScript engine.  To run a test, first load this file,
 * then load the test script.
 */

var completed = false;
var testcases;
var tc = 0;

SECTION = "";
VERSION = "";
BUGNUMBER="";

TZ_DIFF = -8;

var DEBUG = false;

var	GLOBAL = "[object global]";
var PASSED = " PASSED!"
var FAILED = " FAILED! expected: ";

function test() {
    for ( tc=0; tc < testcases.length; tc++ ) {
        testcases[tc].passed = writeTestCaseResult(
                            testcases[tc].expect,
                            testcases[tc].actual,
                            testcases[tc].description +" = "+
                            testcases[tc].actual );

        testcases[tc].reason += ( testcases[tc].passed ) ? "" : "wrong value ";
    }
    stopTest();
    return ( testcases );
}

function TestCase( n, d, e, a ) {
    this.name        = n;
    this.description = d;
    this.expect      = e;
    this.actual      = a;
    this.passed      = true;
    this.reason      = "";
    this.bugnumber   = BUGNUMBER;

    this.passed = getTestCaseResult( this.expect, this.actual );
    if ( DEBUG ) {
        writeLineToLog( "added " + this.description );
    }
}
function startTest() {
    if ( version ) {
        //  JavaScript 1.3 is supposed to be compliant ecma version 1.0
        if ( VERSION == "ECMA_1" ) {
            version ( "130" );
        }
        if ( VERSION == "JS_13" ) {
            version ( "130" );
        }
        if ( VERSION == "JS_12" ) {
            version ( "120" );
        }
        if ( VERSION  == "JS_11" ) {
            version ( "110" );
        }
    }

    // for ecma version 2.0, we will leave the javascript version to
    // the default ( for now ).

    writeHeaderToLog( SECTION + " "+ TITLE);
    if ( BUGNUMBER ) {
        writeLineToLog ("BUGNUMBER: " + BUGNUMBER );
    }

    testcases = new Array();
    tc = 0;
}

function getTestCaseResult( expect, actual ) {
    //  because ( NaN == NaN ) always returns false, need to do
    //  a special compare to see if we got the right result.
        if ( actual != actual ) {
            if ( typeof actual == "object" ) {
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

    //  if both objects are numbers
    // need to replace w/ IEEE standard for rounding
        if (    !passed
                && typeof(actual) == "number"
                && typeof(expect) == "number"
            ) {
                if ( Math.abs(actual-expect) < 0.0000001 ) {
                    passed = true;
                }
        }

    //  verify type is the same
        if ( typeof(expect) != typeof(actual) ) {
            passed = false;
        }

        return passed;
}

function writeTestCaseResult( expect, actual, string ) {
        var passed = getTestCaseResult( expect, actual );
        writeFormattedResult( expect, actual, string, passed );
        return passed;
}

function writeFormattedResult( expect, actual, string, passed ) {
        var s = string ;
        s += ( passed ) ? PASSED : FAILED + expect;
        writeLineToLog( s);
        return passed;
}

function writeLineToLog( string ) {
    print( string  );
}
function writeHeaderToLog( string ) {
    print( string );
}
function stopTest() {
    var gc;
    if ( gc != undefined ) {
        gc();
    }
}
function getFailedCases() {
  for ( var i = 0; i < testcases.length; i++ ) {
     if ( ! testcases[i].passed ) {
        print( testcases[i].description +" = " +testcases[i].actual +" expected: "+ testcases[i].expect );
     }
  }
}
function err( msg, page, line ) {
    writeLineToLog( page + " failed with error: " + msg + " on line " + line );
    testcases[tc].actual = "error";
    testcases[tc].reason = msg;
    writeTestCaseResult( testcases[tc].expect,
                         testcases[tc].actual,
                         testcases[tc].description +" = "+ testcases[tc].actual +
                         ": " + testcases[tc].reason );
    stopTest();
    return true;
}

function Enumerate ( o ) {
    var properties = new Array();
    for ( p in o ) {
       properties[ properties.length ] = new Array( p, o[p] );
    }
    return properties;
}

function getFailedCases() {
  for (	var	i =	0; i < testcases.length; i++ ) {
	 if	( !	testcases[i].passed	) {
		writeLineToLog( testcases[i].description	+" = " +testcases[i].actual	+
		    " expected: "+	testcases[i].expect	);
	 }
  }
}
function AddTestCase( description, expect, actual ) {
    testcases[tc++] = new TestCase( SECTION, description, expect, actual );
}