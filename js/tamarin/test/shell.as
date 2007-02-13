/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2005-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
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
/*
 * JavaScript shared functions file for running the tests in either
 * stand-alone JavaScript engine.  To run a test, first load this file,
 * then load the test script.
 */

import flash.system.*;
import flash.display.*;
import flash.util.*;

var	completed =	false;
var	testcases; // ISSUE:: need to know why we need to comment this out
var tc = 0;
//var version; // ISSUE:: need to know why we need to comment this out

SECTION	= "";
VERSION	= "";
var BUGNUMBER =	"";
var STATUS = "STATUS: ";

//  constant strings

var	GLOBAL = "[object global]";
var PASSED = " PASSED!"
var FAILED = " FAILED! expected: ";
var PACKAGELIST = "{public,$1$private,flash.system,flash.display,flash.util}::";
var TYPEERROR = "TypeError: Error #";
var REFERENCEERROR = "ReferenceError: Error #";
var RANGEERROR = "RangeError: Error #";
var URIERROR = "URIError: Error #";
var EVALERROR = "EvalError: Error #";
var VERIFYERROR = "VerifyError: Error #";
var VERBOSE = true;

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

function evalError( str ){
	return str.slice(0,EVALERROR.length+4);
}

function verifyError( str ){
	return str.slice(0,VERIFYERROR.length+4);
}

// wrapper for test cas constructor that doesn't require the SECTION
//  argument.


function AddTestCase( description, expect, actual ) {
    testcases[tc++] = new TestCase( SECTION, description, expect, actual );
}


//  TestCase constructor


function TestCase( n, d, e,	a )	{
	this.name		 = n;
	this.description = d;
	this.expect		 = e;
	this.actual		 = a;
	this.passed		 = true;
	this.reason		 = "";
	//this.bugnumber	  =	BUGNUMBER;

	this.passed	= getTestCaseResult( this.expect, this.actual );
	if ( DEBUG ) {
		writeLineToLog(	"added " + this.description	);
	}
}


//  Set up test environment.


function startTest() {

    // print out bugnumber
    /*if ( BUGNUMBER ) {
            writeLineToLog ("BUGNUMBER: " + BUGNUMBER );
    }*/

    testcases = new Array();
    tc = 0;
}


function test(... rest:Array) {

	if( rest.length == 0 ){
		// no args sent, use default test
    	for ( tc=0; tc < testcases.length; tc++ ) {
        	testcases[tc].passed = writeTestCaseResult(
                            	testcases[tc].expect,
                            	testcases[tc].actual,
                            	testcases[tc].description +" = "+ testcases[tc].actual );
        	testcases[tc].reason += ( testcases[tc].passed ) ? "" : "wrong value ";
    	}
	} else {
		// we need to use a specialized call to writeTestCaseResult
		if( rest[0] == "no actual" ){
    		for ( tc=0; tc < testcases.length; tc++ ) {
        		testcases[tc].passed = writeTestCaseResult(
                            		testcases[tc].expect,
                            		testcases[tc].actual,
                            		testcases[tc].description );
        		testcases[tc].reason += ( testcases[tc].passed ) ? "" : "wrong value ";
    	}
		}
	}
    stopTest();
    return ( testcases );
}


//  Compare expected result to the actual result and figure out whether
//  the test case passed.

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

//  Begin printing functions.  These functions use the shell's
//  print function.  When running tests in the browser, these
//  functions, override these functions with functions that use
//  document.write.

function writeTestCaseResult( expect, actual, string ) {
		var	passed = getTestCaseResult(	expect,	actual );
        writeFormattedResult( expect, actual, string, passed );
		return passed;
}
function writeFormattedResult( expect, actual, string, passed ) {
        var s = string ;
        s += ( passed ) ? PASSED : FAILED + expect;
        writeLineToLog( s);
        return passed;
}
function writeLineToLog( string	) {
	trace( string );
}
function writeHeaderToLog( string )	{
	trace( string );
}
// end of print functions



//  When running in the shell, run the garbage collector after the
//  test has completed.


function stopTest()	{
}


//  	Date functions used	by tests in	Date suite

//  Originally, the test suite used a hard-coded value TZ_DIFF = -8.
//  But that was only valid for testers in the Pacific Standard Time Zone!
//  We calculate the proper number dynamically for any tester. We just
//  have to be careful not to use a date subject to Daylight Savings Time...

function getTimeZoneDiff()
{
  // re-hard coding to -8 because Date object is not implemented yet
  // mtilburg 3/15/2004
  //return -8;
  return -((new Date(2000, 1, 1)).getTimezoneOffset())/60;
}

var	msPerDay =			86400000;
var	HoursPerDay	=		24;
var	MinutesPerHour =	60;
var	SecondsPerMinute =	60;
var	msPerSecond	=		1000;
var	msPerMinute	=		60000;		//	msPerSecond	* SecondsPerMinute
var	msPerHour =			3600000;	//	msPerMinute	* MinutesPerHour
var TZ_DIFF	= getTimeZoneDiff();  // offset of tester's timezone from UTC
var             TZ_PST = -8;  // offset of Pacific Standard Time from UTC
var             PST_DIFF = TZ_DIFF - TZ_PST;  // offset of tester's timezone from PST
var	TIME_1970	 = 0;
var	TIME_2000	 = 946684800000;
var	TIME_1900	 = -2208988800000;
var now = new Date();
var TZ_DIFF = getTimeZoneDiff();
var TZ_ADJUST = TZ_DIFF * msPerHour;
var UTC_29_FEB_2000 = TIME_2000 + 31*msPerDay + 28*msPerDay;
var UTC_1_JAN_2005 = TIME_2000 + TimeInYear(2000) + TimeInYear(2001) +
                     TimeInYear(2002) + TimeInYear(2003) + TimeInYear(2004);
var TIME_NOW = now.valueOf();





//  Date test "ResultArrays" are hard-coded for Pacific Standard Time.
//  We must adjust them for the tester's own timezone -

function adjustResultArray(ResultArray, msMode)
{
  // If the tester's system clock is in PST, no need to continue -
  if (!PST_DIFF) {return;}

  // The date testcases instantiate Date objects in two different ways:
  //
  //         millisecond mode: e.g.   dt = new Date(10000000);
  //         year-month-day mode:  dt = new Date(2000, 5, 1, ...);
  //
  //  In the first case, the date is measured from Time 0 in Greenwich (i.e. UTC).
  //  In the second case, it is measured with reference to the tester's local timezone.
  //
  //  In the first case we must correct those values expected for local measurements,
  //  like dt.getHours() etc. No correction is necessary for dt.getUTCHours() etc.
  //
  //  In the second case, it is exactly the other way around -

  var t;
  if (msMode)
  {
    // The hard-coded UTC milliseconds from Time 0 derives from a UTC date.
    // Shift to the right by the offset between UTC and the tester.
    t = ResultArray[TIME]  +  TZ_DIFF*msPerHour;

    // Use our date arithmetic functions to determine the local hour, day, etc.
    ResultArray[HOURS] = HourFromTime(t);
    ResultArray[DAY] = WeekDay(t);
    ResultArray[DATE] = DateFromTime(t);
    ResultArray[MONTH] = MonthFromTime(t);
    ResultArray[YEAR] = YearFromTime(t);
  }
  else
  {
    // The hard-coded UTC milliseconds from Time 0 derives from a PST date.
    // Shift to the left by the offset between PST and the tester.
    t = ResultArray[TIME]  -  PST_DIFF*msPerHour;

    // Use our date arithmetic functions to determine the UTC hour, day, etc.
    ResultArray[TIME] = t;
    ResultArray[UTC_HOURS] = HourFromTime(t);
    ResultArray[UTC_DAY] = WeekDay(t);
    ResultArray[UTC_DATE] = DateFromTime(t);
    ResultArray[UTC_MONTH] = MonthFromTime(t);
    ResultArray[UTC_YEAR] = YearFromTime(t);
  }
}


function Day( t	) {
	return ( Math.floor(t/msPerDay ) );
}
function DaysInYear( y ) {
	if ( y % 4 != 0	) {
		return 365;
	}
	if ( (y	% 4	== 0) && (y	% 100 != 0)	) {
		return 366;
	}
	if ( (y	% 100 == 0)	&&	(y % 400 !=	0) ) {
		return 365;
	}
	if ( (y	% 400 == 0)	){
		return 366;
	} else {
		trace("ERROR: DaysInYear("	+ y	+ ") case not covered");
		return Math.NaN; //"ERROR: DaysInYear("	+ y	+ ") case not covered";
	}
}
function TimeInYear( y ) {
	return ( DaysInYear(y) * msPerDay );
}
function DayNumber(	t )	{
	return ( Math.floor( t / msPerDay )	);
}
function TimeWithinDay(	t )	{
	if ( t < 0 ) {
		return ( (t	% msPerDay)	+ msPerDay );
	} else {
		return ( t % msPerDay );
	}
}
function YearNumber( t ) {
}
function TimeFromYear( y ) {
	return ( msPerDay *	DayFromYear(y) );
}
function DayFromYear( y	) {
	return (	365*(y-1970) +
				Math.floor((y-1969)/4) -
				Math.floor((y-1901)/100) +
				Math.floor((y-1601)/400) );
}
function InLeapYear( t ) {
	if ( DaysInYear(YearFromTime(t)) ==	365	) {
		return 0;
	}
	if ( DaysInYear(YearFromTime(t)) ==	366	) {
		return 1;
	} else {
		return "ERROR:  InLeapYear("+ t + ") case not covered";
	}
}
function YearFromTime( t ) {
	t =	Number(	t );
	var	sign = ( t < 0 ) ? -1 :	1;
	var	year = ( sign <	0 )	? 1969 : 1970;

	for	(	var	timeToTimeZero = t;	;  ) {
	//	subtract the current year's	time from the time that's left.
		timeToTimeZero -= sign * TimeInYear(year)
		if (isNaN(timeToTimeZero))
			return NaN;

	//	if there's less	than the current year's	worth of time left,	then break.
		if ( sign <	0 )	{
			if ( sign *	timeToTimeZero <= 0	) {
				break;
			} else {
				year +=	sign;
			}
		} else {
			if ( sign *	timeToTimeZero < 0 ) {
				break;
			} else {
				year +=	sign;
			}
		}
	}
	return ( year );
}
function MonthFromTime(	t )	{
	//	i know i could use switch but i'd rather not until it's	part of	ECMA
	var	day	= DayWithinYear( t );
	var	leap = InLeapYear(t);

	if ( (0	<= day)	&& (day	< 31) )	{
		return 0;
	}
	if ( (31 <=	day) &&	(day < (59+leap)) )	{
		return 1;
	}
	if ( ((59+leap)	<= day)	&& (day	< (90+leap)) ) {
		return 2;
	}
	if ( ((90+leap)	<= day)	&& (day	< (120+leap)) )	{
		return 3;
	}
	if ( ((120+leap) <=	day) &&	(day < (151+leap)) ) {
		return 4;
	}
	if ( ((151+leap) <=	day) &&	(day < (181+leap)) ) {
		return 5;
	}
	if ( ((181+leap) <=	day) &&	(day < (212+leap)) ) {
		return 6;
	}
	if ( ((212+leap) <=	day) &&	(day < (243+leap)) ) {
		return 7;
	}
	if ( ((243+leap) <=	day) &&	(day < (273+leap)) ) {
		return 8;
	}
	if ( ((273+leap) <=	day) &&	(day < (304+leap)) ) {
		return 9;
	}
	if ( ((304+leap) <=	day) &&	(day < (334+leap)) ) {
		return 10;
	}
	if ( ((334+leap) <=	day) &&	(day < (365+leap)) ) {
		return 11;
	} else {
		return "ERROR:	MonthFromTime("+t+") not known";
	}
}
function DayWithinYear(	t )	{
		return(	Day(t) - DayFromYear(YearFromTime(t)));
}
function DateFromTime( t ) {
	var	day	= DayWithinYear(t);
	var	month =	MonthFromTime(t);

	if ( month == 0	) {
		return ( day + 1 );
	}
	if ( month == 1	) {
		return ( day - 30 );
	}
	if ( month == 2	) {
		return ( day - 58 -	InLeapYear(t) );
	}
	if ( month == 3	) {
		return ( day - 89 -	InLeapYear(t));
	}
	if ( month == 4	) {
		return ( day - 119 - InLeapYear(t));
	}
	if ( month == 5	) {
		return ( day - 150-	InLeapYear(t));
	}
	if ( month == 6	) {
		return ( day - 180-	InLeapYear(t));
	}
	if ( month == 7	) {
		return ( day - 211-	InLeapYear(t));
	}
	if ( month == 8	) {
		return ( day - 242-	InLeapYear(t));
	}
	if ( month == 9	) {
		return ( day - 272-	InLeapYear(t));
	}
	if ( month == 10 ) {
		return ( day - 303-	InLeapYear(t));
	}
	if ( month == 11 ) {
		return ( day - 333-	InLeapYear(t));
	}

	return ("ERROR:	 DateFromTime("+t+") not known"	);
}
function WeekDay( t	) {
	var	weekday	= (Day(t)+4) % 7;
	return(	weekday	< 0	? 7	+ weekday :	weekday	);
}

// missing daylight	savins time	adjustment

function HourFromTime( t ) {
	var	h =	Math.floor(	t /	msPerHour )	% HoursPerDay;
	return ( (h<0) ? HoursPerDay + h : h  );
}
function MinFromTime( t	) {
	var	min	= Math.floor( t	/ msPerMinute )	% MinutesPerHour;
	return(	( min <	0 )	? MinutesPerHour + min : min  );
}
function SecFromTime( t	) {
	var	sec	= Math.floor( t	/ msPerSecond )	% SecondsPerMinute;
	return ( (sec <	0 )	? SecondsPerMinute + sec : sec );
}
function msFromTime( t ) {
	var	ms = t % msPerSecond;
	return ( (ms < 0 ) ? msPerSecond + ms :	ms );
}
function LocalTZA()	{
	return ( TZ_DIFF * msPerHour );
}
function UTC( t	) {
	return ( t - LocalTZA()	- DaylightSavingTA(t - LocalTZA()) );
}

function DaylightSavingTA( t ) {
	t =	t -	LocalTZA();

	var	dst_start =	GetFirstSundayInApril(t) + 2*msPerHour;
	var	dst_end	  =	GetLastSundayInOctober(t)+ 2*msPerHour;

	if ( t >= dst_start	&& t < dst_end ) {
		return msPerHour;
	} else {
		return 0;
	}

	// Daylight	Savings	Time starts	on the first Sunday	in April at	2:00AM in
	// PST.	 Other time	zones will need	to override	this function.
	trace( new Date( UTC(dst_start + LocalTZA())) );

	return UTC(dst_start  +	LocalTZA());
}
function GetFirstSundayInApril(	t )	{
	var	year = YearFromTime(t);
	var	leap = InLeapYear(t);

	var	april =	TimeFromYear(year) + TimeInMonth(0,	leap) +	TimeInMonth(1,leap)	+
	TimeInMonth(2,leap);

	for	( var first_sunday = april;	WeekDay(first_sunday) >	0;
		first_sunday +=	msPerDay )
	{
		;
	}

	return first_sunday;
}
function GetLastSundayInOctober( t ) {
	var	year = YearFromTime(t);
	var	leap = InLeapYear(t);

	for	( var oct =	TimeFromYear(year),	m =	0; m < 9; m++ )	{
		oct	+= TimeInMonth(m, leap);
	}
	for	( var last_sunday =	oct	+ 30*msPerDay; WeekDay(last_sunday)	> 0;
		last_sunday	-= msPerDay	)
	{
		;
	}
	return last_sunday;
}
function LocalTime(	t )	{
	return ( t + LocalTZA()	+ DaylightSavingTA(t) );
}
function MakeTime( hour, min, sec, ms )	{
	if ( isNaN(	hour ) || isNaN( min ) || isNaN( sec ) || isNaN( ms	) )	{
		return Number.NaN;
	}

	hour = ToInteger(hour);
	min	 = ToInteger( min);
	sec	 = ToInteger( sec);
	ms	 = ToInteger( ms );

	return(	(hour*msPerHour) + (min*msPerMinute) +
			(sec*msPerSecond) +	ms );
}
function MakeDay( year,	month, date	) {
	if ( isNaN(year) ||	isNaN(month) ||	isNaN(date)	) {
		return Number.NaN;
	}
	year = ToInteger(year);
	month =	ToInteger(month);
	date = ToInteger(date );

	var	sign = ( year <	1970 ) ? -1	: 1;
	var	t =	   ( year <	1970 ) ? 1 :  0;
	var	y =	   ( year <	1970 ) ? 1969 :	1970;

	var	result5	= year + Math.floor( month/12 );
	var	result6	= month	% 12;

	if ( year <	1970 ) {
	   for ( y = 1969; y >=	year; y	+= sign	) {
		 t += sign * TimeInYear(y);
	   }
	} else {
		for	( y	= 1970 ; y < year; y +=	sign ) {
			t += sign *	TimeInYear(y);
		}
	}

	var	leap = InLeapYear( t );

	for	( var m	= 0; m < month;	m++	) {
		t += TimeInMonth( m, leap );
	}

	if ( YearFromTime(t) !=	result5	) {
		return Number.NaN;
	}
	if ( MonthFromTime(t) != result6 ) {
		return Number.NaN;
	}
	if ( DateFromTime(t) !=	1 )	{
		return Number.NaN;
	}

	return ( (Day(t)) +	date - 1 );
}
function TimeInMonth( month, leap )	{
	// september april june	november
	// jan 0  feb 1	 mar 2	apr	3	may	4  june	5  jul 6
	// aug 7  sep 8	 oct 9	nov	10	dec	11

	if ( month == 3	|| month ==	5 || month == 8	|| month ==	10 ) {
		return ( 30*msPerDay );
	}

	// all the rest
	if ( month == 0	|| month ==	2 || month == 4	|| month ==	6 ||
		 month == 7	|| month ==	9 || month == 11 ) {
		return ( 31*msPerDay );
	 }

	// save	february
	return ( (leap == 0) ? 28*msPerDay : 29*msPerDay );
}
function MakeDate( day,	time ) {
	if (	day	== Number.POSITIVE_INFINITY	||
			day	== Number.NEGATIVE_INFINITY	||
			day	== Number.NaN )	{
		return Number.NaN;
	}
	if (	time ==	Number.POSITIVE_INFINITY ||
			time ==	Number.POSITIVE_INFINITY ||
			day	== Number.NaN) {
		return Number.NaN;
	}
	return ( day * msPerDay	) +	time;
}
function TimeClip( t ) {
	if ( isNaN(	t )	) {
		return ( Number.NaN	);
	}
	if ( Math.abs( t ) > 8.64e15 ) {
		return ( Number.NaN	);
	}

	return ( ToInteger(	t )	);
}
function ToInteger(	t )	{
	t =	Number(	t );

	if ( isNaN(	t )	){
		return ( Number.NaN	);
	}
	if ( t == 0	|| t ==	-0 ||
		 t == Number.POSITIVE_INFINITY || t	== Number.NEGATIVE_INFINITY	) {
		 return	0;
	}

	var	sign = ( t < 0 ) ? -1 :	1;

	return ( sign *	Math.floor(	Math.abs( t	) )	);
}
function Enumerate ( o ) {
	var	p;
	for	( p	in o ) {
		trace( p +": " + o[p] );
	}
}



function START(summary)
{
      // print out bugnumber

     /*if ( BUGNUMBER ) {
              writeLineToLog ("BUGNUMBER: " + BUGNUMBER );
      }*/
    XML.setSettings (null);
    testcases = new Array();

    // text field for results
    tc = 0;
    /*this.addChild ( tf );
    tf.x = 30;
    tf.y = 50;
    tf.width = 200;
    tf.height = 400;*/

    trace(summary);
    var summaryParts = summary.split(" ");
    trace("section: " + summaryParts[0] + "!");
    //fileName = summaryParts[0];

}

function BUG(bug)
{
  printBugNumber(bug);
}

function TEST(section, expected, actual)
{
	/*if( actual == "this will never match"){
		// hack for strict mode
		var fileName;
	}*/


    // Testcase Description
    this[fileName+"Str"].push(section);

    // Testcase Actual Values Returned
    this[fileName].push(actual);

    // Testcase Expected Values Returned
    this[fileName+"Ans"].push(expected);

    var expected_t = typeof expected;
    var actual_t = typeof actual;
    var output = "";

    // Testcase Description
    this[fileName+"Str"].push(section + "type test");

    // Testcase Actual Values Returned
    this[fileName].push(actual_t);

    // Testcase Expected Values Returned
    this[fileName+"Ans"].push(expected_t);

    if (expected_t != actual_t)
        output += "Type mismatch, expected type " + expected_t +
            ", actual type " + actual_t + "\n";
    else if (VERBOSE)
        printStatus ("Expected type '" + actual_t + "' matched actual " +
                     "type '" + expected_t + "'");

    if (expected != actual) {
	output += "Expected value:\n" + toPrinted(expected) + "\nActual value:\n" + toPrinted(actual) + "\n";
    }
    else if (VERBOSE)
        printStatus ("Expected value '" + toPrinted(actual) + "' matched actual value");

    if (output != "")
    {
        reportFailure (output);
    }
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
    reportFailure ("Bad TEST_XML usage: type of expected is "+expected_t+", should be number or string");
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

function printBugNumber (num)
{
  //writeLineToLog (BUGNUMBER + num);
}

function toPrinted(value)
{
  if (typeof value == "xml") {
    return value.toXMLString();
  } else {
    return String(value);
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
	var num;
	if (numIndex >= 0) {
		num = str.substr(numIndex, 11);
	} else {
		num = str;
	}
	return num;
}
var cnNoObject = 'Unexpected Error!!! Parameter to this function must be an object';
var cnNoClass = 'Unexpected Error!!! Cannot find Class property';
var cnObjectToString = Object.prototype.toString;

// checks that it's safe to call findType()
function getJSType(obj)
{
  if (isObject(obj))
    return findType(obj);
  return cnNoObject;
}


// checks that it's safe to call findType()
function getJSClass(obj)
{
  if (isObject(obj))
    return findClass(findType(obj));
  return cnNoObject;
}
function isObject(obj)
{
  return obj instanceof Object;
}

function findType(obj)
{
  return cnObjectToString.apply(obj);
}
// given '[object Number]',  return 'Number'
function findClass(sType)
{
  var re =  /^\[.*\s+(\w+)\s*\]$/;
  var a = sType.match(re);

  if (a && a[1])
    return a[1];
  return cnNoClass;
}
function inSection(x) {
   return "Section "+x+" of test -";
}
function printStatus (msg)
{
    var lines = msg.split ("\n");
    var l;

    for (var i=0; i<lines.length; i++)
        trace(STATUS + lines[i]);

}
function reportCompare (expected, actual, description)
{
    var expected_t = typeof expected;
    var actual_t = typeof actual;
    var output = "";
   	if ((VERBOSE) && (typeof description != "undefined"))
            printStatus ("Comparing '" + description + "'");

    if (expected_t != actual_t)
            output += "Type mismatch, expected type " + expected_t +
                ", actual type " + actual_t + "\n";
    else if (VERBOSE)
            printStatus ("Expected type '" + actual_t + "' matched actual " +
                         "type '" + expected_t + "'");

    if (expected != actual)
            output += "Expected value '" + expected + "', Actual value '" + actual +
                "'\n";
    else if (VERBOSE)
            printStatus ("Expected value '" + actual + "' matched actual " +
                         "value '" + expected + "'");

    if (output != "")
        {
            if (typeof description != "undefined")
                reportFailure (description);
            	reportFailure (output);
        }
    stopTest();
}
