/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1997-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Igor Bukanov
 *   Rob Ginda rginda@netscape.com
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 
var FAILED = " FAILED! expected: ";
var PASSED = " PASSED! ";
var BUGNUMBER = "";
var VERBOSE = true;
var SECTION	= "";
var VERSION	= "";
var GLOBAL = "[object global]";
var SECT_PREFIX = 'Section ';
var SECT_SUFFIX = ' of test -';

var completed =	false;
var version;
var testcases = new Array();
var tc = 0;

var defaultSettings = XML.defaultSettings();
XML.setSettings(defaultSettings);


var TYPEERROR = "TypeError: Error #";
var REFERENCEERROR = "ReferenceError: Error #";
var RANGEERROR = "RangeError: Error #";
var URIERROR = "URIError: Error #";

var    DEBUG =	false;

function typeError( str ){
	return str.slice(0,TYPEERROR.length+4);
}

function referenceError( str ){
	return str.slice(0,REFERENCEERROR.length+4);
}
function rangeError( str ){
	return str.slice(0,RANGEERROR.length+4);
}
function uriError( str ){
	return str.slice(0,URIERROR.length+4);
}

/*
 * The test driver searches for such a phrase in the test output.
 * If such phrase exists, it will set n as the expected exit code.
 */
function expectExitCode(n)
{

    trace('--- NOTE: IN THIS TESTCASE, WE EXPECT EXIT CODE ' + n + ' ---');

}

/*
 * Statuses current section of a test
 */
function inSection(x)
{
    return SECT_PREFIX + x + SECT_SUFFIX;
}

/*
 * Report a failure in the 'accepted' manner
 */
function reportFailure (section, msg)
{
    trace(FAILED + inSection(section)+"\n"+msg);
    /*var lines = msg.split ("\n");
    for (var i=0; i<lines.length; i++)
        trace (FAILED + lines[i]);
    */
}

/*
 * trace a non-failure message.
 */
function printStatus (msg)
{
    trace(PASSED + msg);
    /*var lines = msg.split ("\n");
    var l;

    for (var i=0; i<lines.length; i++)
        trace (PASSED + lines[i]);
    */
}

/*
 * trace a bugnumber message.
 */
function printBugNumber (num)
{
  trace (BUGNUMBER + num);
}

function toPrinted(value)
{
  if (typeof value == "xml") {
    return value.toXMLString();
  } else {
    return String(value);
  }
}

function START(summary)
{
  if ( version ) {
      	//	JavaScript 1.3 is supposed to be compliant ecma	version	1.0
  	    if ( VERSION ==	"ECMA_1" ) {
  		    version	( "130"	);
      	}
  	    if ( VERSION ==	"JS_1.3" ) {
  		    version	( "130"	);
      	}
  	    if ( VERSION ==	"JS_1.2" ) {
  		    version	( "120"	);
      	}
  	    if ( VERSION  == "JS_1.1" )	{
  		    version	( "110"	);
      	}
  	    // for ecma	version	2.0, we	will leave the javascript version to
      	// the default ( for now ).
      }
  
      // trace out bugnumber
  
      if ( BUGNUMBER ) {
              writeLineToLog ("BUGNUMBER: " + BUGNUMBER );
      }
  
    testcases = new Array();
    tc = 0;
  
    trace(summary);
  
}

function BUG(bug)
{
  printBugNumber(bug);
}

function TEST(section, expected, actual)
{
    var expected_t = typeof expected;
    var actual_t = typeof actual;
    AddTestCase("Type check", expected_t, actual_t); 	
    AddTestCase(section, expected, actual);
}

function TEST_XML(section, expected, actual)
{
  var actual_t = typeof actual;
  var expected_t = typeof expected;

  if (actual_t != "xml") {
    // force error on type mismatch
    TEST(section, new XML(), actual);
    return;
  }
  
  if (expected_t == "string") {
    TEST(section, expected, actual.toXMLString());
  } else if (expected_t == "number") {
    TEST(section, String(expected), actual.toXMLString());
  } else {
    reportFailure (section, "Bad TEST_XML usage: type of expected is "+expected_t+", should be number or string");
  }
}

function SHOULD_THROW(section)
{
  reportFailure(section, "Expected to generate exception, actual behavior: no exception was thrown");   
}

function END()
{
	test();
}

function NL() 
{
  //return java.lang.System.getProperty("line.separator");
  return "\n";
}


/* 
*
*
* FUNCTIONS ADDED BY VERA FLEISCHER
*
*/

/* wrapper for test cas constructor that doesn't require the SECTION
 * argument.
 */

function AddTestCase( description, expect, actual ) {
    testcases[tc++] = new TestCase( "blah section", description, expect, actual );
}

/*
 * TestCase constructor
 *
 */

function TestCase( n, d, e,	a )	{
	this.name		 = n;
	this.description = d;
	this.expect		 = e;
	this.actual		 = a;
	this.passed		 = true;
	this.reason		 = "";
	this.bugnumber	  =	BUGNUMBER;

	this.passed	= getTestCaseResult( this.expect, this.actual );
	/*if ( DEBUG ) {
		writeLineToLog(	"added " + this.description	);
	}*/
}

function test() {
    for ( tc=0; tc < testcases.length; tc++ ) {
        testcases[tc].passed = writeTestCaseResult(
                            testcases[tc].expect,
                            testcases[tc].actual,
                            testcases[tc].description +" = "+ testcases[tc].actual );
        testcases[tc].reason += ( testcases[tc].passed ) ? "" : "wrong value ";
    }
    stopTest();
    return ( testcases );
}

/*
 * Compare expected result to the actual result and figure out whether
 * the test case passed.
 */
function getTestCaseResult(	expect,	actual ) {
	//	because	( NaN == NaN ) always returns false, need to do
	//	a special compare to see if	we got the right result.
		if ( actual	!= actual )	{
			if ( typeof	actual == "object" ) {
				actual = "NaN object";
			} else {
				actual = "NaN number";
			}
		}
		if ( expect	!= expect )	{
			if ( typeof	expect == "object" ) {
				expect = "NaN object";
			} else {
				expect = "NaN number";
			}
		}

		var	passed = ( expect == actual	) ?	true : false;

	//	if both	objects	are	numbers
	// need	to replace w/ IEEE standard	for	rounding
		if (	!passed
				&& typeof(actual) == "number"
				&& typeof(expect) == "number"
			) {
				if ( Math.abs(actual-expect) < 0.0000001 ) {
					passed = true;
				}
		}

	//	verify type	is the same
		if ( typeof(expect)	!= typeof(actual) )	{
			passed = false;
		}

		return passed;
}

/*
 * Begin printing functions.  These functions use the shell's
 * print function.  When running tests in the browser, these
 * functions, override these functions with functions that use
 * document.write.
 */

function writeTestCaseResult( expect, actual, string ) {

		var	passed = getTestCaseResult(	expect,	actual );
    	writeFormattedResult( expect, actual, string, passed );
		return passed;
}
function writeFormattedResult( expect, actual, string, passed ) {
        var s = ( passed ) ? PASSED : FAILED + expect + ", ";
        s += string;
        writeLineToLog( s);
        return passed;
}
function writeLineToLog( string	) {
	trace( string );
}
function writeHeaderToLog( string )	{
	trace( string );
}
/* end of trace functions */


/*
 * When running in the shell, run the garbage collector after the
 * test has completed.
 */

function stopTest()	{
 	var gc;
	if ( gc != undefined ) {
		gc();
	}
}

function grabError(err, str) {
	var typeIndex = str.indexOf("Error:");
	var type = str.substr(0, typeIndex + 5);
	if (type == "TypeError") {
		AddTestCase("Asserting for TypeError", true, (err is TypeError));
	} else if (type == "ArgumentError") {
		AddTestCase("Asserting for ArgumentError", true, (err is ArgumentError));
	}
	var numIndex = str.indexOf("Error #");
	if (numIndex >= 0) {
		num = str.substr(numIndex, 11);
	} else {
		num = str;
	}
	return num;
}
function myGetNamespace (obj, ns) {
	if (ns != undefined) {
		return obj.namespace(ns);
	} else {
		return obj.namespace();
	}
}