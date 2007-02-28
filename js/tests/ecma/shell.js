/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

var completed = false;
var testcases = new Array();
var tc = testcases.length; 

var SECTION = "";
var VERSION = "";
var BUGNUMBER = "";

/*
 * constant strings
 */
var GLOBAL = "[object global]";
var PASSED = " PASSED!"
var FAILED = " FAILED! expected: ";

var DEBUG = false;

var DESCRIPTION;
var EXPECTED;

/* 
 * wrapper for test case constructor that doesn't require the SECTION
 * argument.
 */

function AddTestCase( description, expect, actual ) {
  new TestCase( SECTION, description, expect, actual );
}

/*
 * TestCase constructor
 *
 */

function TestCase( n, d, e, a ) {
  this.path = (typeof gTestPath == 'undefined') ? '' : gTestPath;
  this.name        = n;
  this.description = d;
  this.expect      = e;
  this.actual      = a;
  this.passed      = true;
  this.reason      = "";
  this.bugnumber   = BUGNUMBER;

  this.passed = getTestCaseResult( this.expect, this.actual );
  if ( DEBUG ) {
    print( "added " + this.description );
  }
  /*
   * testcases are solely maintained in the TestCase
   * constructor. tc will _always_ point to one past the
   * last testcase. If an exception occurs during the call
   * to the constructor, then we are assured that the tc
   * index has not been incremented.
   */
  
  testcases[tc++] = this;
}

/*
 * Set up test environment.
 *
 */
function startTest() {
  if ( version ) {
    // JavaScript 1.3 is supposed to be compliant ecma version 1.0
    if ( VERSION == "ECMA_1" ) {
      version ( "130" );
    }
    if ( VERSION == "JS_1.3" ) {
      version ( "130" );
    }
    if ( VERSION == "JS_1.2" ) {
      version ( "120" );
    }
    if ( VERSION  == "JS_1.1" ) {
      version ( "110" );
    }
    // for ecma version 2.0, we will leave the javascript version to
    // the default ( for now ).
  }

  // print out bugnumber

  if ( BUGNUMBER ) {
    print ("BUGNUMBER: " + BUGNUMBER );
  }
}

function test() {
  for ( tc=0; tc < testcases.length; tc++ ) {
    // temporary hack to work around some unknown issue in 1.7
    try
    {
    testcases[tc].passed = writeTestCaseResult(
      testcases[tc].expect,
      testcases[tc].actual,
      testcases[tc].description +" = "+ testcases[tc].actual );
    testcases[tc].reason += ( testcases[tc].passed ) ? "" : "wrong value ";
    }
    catch(e)
    {
      print('test(): empty testcase for tc = ' + tc + ' ' + e);
    }
  }
  stopTest();
  return ( testcases );
}

/*
 * Compare expected result to the actual result and figure out whether
 * the test case passed.
 */
function getTestCaseResult( expect, actual ) {
  // because ( NaN == NaN ) always returns false, need to do
  // a special compare to see if we got the right result.
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

  // if both objects are numbers
  // need to replace w/ IEEE standard for rounding
  if ( !passed
       && typeof(actual) == "number"
       && typeof(expect) == "number"
    ) {
    if ( Math.abs(actual-expect) < 0.0000001 ) {
      passed = true;
    }
  }

  // verify type is the same
  if ( typeof(expect) != typeof(actual) ) {
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
  var passed = getTestCaseResult( expect, actual );
  writeFormattedResult( expect, actual, string, passed );
  return passed;
}
function writeFormattedResult( expect, actual, string, passed ) {
  var s = string ;
  s += ( passed ) ? PASSED : FAILED + expect;
  print( s);
  return passed;
}

function writeHeaderToLog( string ) {
  print( string );
}
/* end of print functions */


/*
 * When running in the shell, run the garbage collector after the
 * test has completed.
 */

function stopTest() {
  var gc;
  if ( gc != undefined ) {
    gc();
  }
}

/*
 * Convenience function for displaying failed test cases.  Useful
 * when running tests manually.
 *
 */
function getFailedCases() {
  for ( var i = 0; i < testcases.length; i++ ) {
    if ( ! testcases[i].passed ) {
      print( testcases[i].description +" = " +testcases[i].actual +" expected: "+ testcases[i].expect );
    }
  }
}
/*
 * Date functions used by tests in Date suite
 *
 */
var msPerDay =   86400000;
var HoursPerDay =  24;
var MinutesPerHour = 60;
var SecondsPerMinute = 60;
var msPerSecond =  1000;
var msPerMinute =  60000;  // msPerSecond * SecondsPerMinute
var msPerHour =   3600000; // msPerMinute * MinutesPerHour
var TZ_DIFF = getTimeZoneDiff();  // offset of tester's timezone from UTC
var TZ_PST = -8;  // offset of Pacific Standard Time from UTC
var PST_DIFF = TZ_DIFF - TZ_PST;  // offset of tester's timezone from PST
var TIME_1970  = 0;
var TIME_2000  = 946684800000;
var TIME_1900  = -2208988800000;


/*
 * Originally, the test suite used a hard-coded value TZ_DIFF = -8. 
 * But that was only valid for testers in the Pacific Standard Time Zone! 
 * We calculate the proper number dynamically for any tester. We just
 * have to be careful not to use a date subject to Daylight Savings Time...
 */
function getTimeZoneDiff()
{
  return -((new Date(2000, 1, 1)).getTimezoneOffset())/60;
}


/* 
 * Date test "ResultArrays" are hard-coded for Pacific Standard Time. 
 * We must adjust them for the tester's own timezone -
 */
function adjustResultArray(ResultArray, msMode)
{
  // If the tester's system clock is in PST, no need to continue - 
  if (!PST_DIFF) {return;} 

  /* The date testcases instantiate Date objects in two different ways:
   *
   *        millisecond mode: e.g.   dt = new Date(10000000);
   *        year-month-day mode:  dt = new Date(2000, 5, 1, ...);
   *
   * In the first case, the date is measured from Time 0 in Greenwich (i.e. UTC).
   * In the second case, it is measured with reference to the tester's local timezone.
   *
   * In the first case we must correct those values expected for local measurements,
   * like dt.getHours() etc. No correction is necessary for dt.getUTCHours() etc.
   * 
   * In the second case, it is exactly the other way around -
   */ 
  if (msMode)
  {
    // The hard-coded UTC milliseconds from Time 0 derives from a UTC date.
    // Shift to the right by the offset between UTC and the tester.
    var t = ResultArray[TIME]  +  TZ_DIFF*msPerHour;

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
    var t = ResultArray[TIME]  -  PST_DIFF*msPerHour;

    // Use our date arithmetic functions to determine the UTC hour, day, etc. 
    ResultArray[TIME] = t;
    ResultArray[UTC_HOURS] = HourFromTime(t); 
    ResultArray[UTC_DAY] = WeekDay(t);
    ResultArray[UTC_DATE] = DateFromTime(t);
    ResultArray[UTC_MONTH] = MonthFromTime(t);
    ResultArray[UTC_YEAR] = YearFromTime(t);
  }
}


function Day( t ) {
  return ( Math.floor(t/msPerDay ) );
}
function DaysInYear( y ) {
  if ( y % 4 != 0 ) {
    return 365;
  }
  if ( (y % 4 == 0) && (y % 100 != 0) ) {
    return 366;
  }
  if ( (y % 100 == 0) && (y % 400 != 0) ) {
    return 365;
  }
  if ( (y % 400 == 0) ){
    return 366;
  } else {
    return "ERROR: DaysInYear(" + y + ") case not covered";
  }
}
function TimeInYear( y ) {
  return ( DaysInYear(y) * msPerDay );
}
function DayNumber( t ) {
  return ( Math.floor( t / msPerDay ) );
}
function TimeWithinDay( t ) {
  if ( t < 0 ) {
    return ( (t % msPerDay) + msPerDay );
  } else {
    return ( t % msPerDay );
  }
}
function YearNumber( t ) {
}
function TimeFromYear( y ) {
  return ( msPerDay * DayFromYear(y) );
}
function DayFromYear( y ) {
  return ( 365*(y-1970) +
           Math.floor((y-1969)/4) -
           Math.floor((y-1901)/100) +
           Math.floor((y-1601)/400) );
}
function InLeapYear( t ) {
  if ( DaysInYear(YearFromTime(t)) == 365 ) {
    return 0;
  }
  if ( DaysInYear(YearFromTime(t)) == 366 ) {
    return 1;
  } else {
    return "ERROR:  InLeapYear("+ t + ") case not covered";
  }
}
function YearFromTime( t ) {
  t = Number( t );
  var sign = ( t < 0 ) ? -1 : 1;
  var year = ( sign < 0 ) ? 1969 : 1970;
  for ( var timeToTimeZero = t; ;  ) {
    // subtract the current year's time from the time that's left.
    timeToTimeZero -= sign * TimeInYear(year)

      // if there's less than the current year's worth of time left, then break.
      if ( sign < 0 ) {
        if ( sign * timeToTimeZero <= 0 ) {
          break;
        } else {
          year += sign;
        }
      } else {
        if ( sign * timeToTimeZero < 0 ) {
          break;
        } else {
          year += sign;
        }
      }
  }
  return ( year );
}
function MonthFromTime( t ) {
  // i know i could use switch but i'd rather not until it's part of ECMA
  var day = DayWithinYear( t );
  var leap = InLeapYear(t);

  if ( (0 <= day) && (day < 31) ) {
    return 0;
  }
  if ( (31 <= day) && (day < (59+leap)) ) {
    return 1;
  }
  if ( ((59+leap) <= day) && (day < (90+leap)) ) {
    return 2;
  }
  if ( ((90+leap) <= day) && (day < (120+leap)) ) {
    return 3;
  }
  if ( ((120+leap) <= day) && (day < (151+leap)) ) {
    return 4;
  }
  if ( ((151+leap) <= day) && (day < (181+leap)) ) {
    return 5;
  }
  if ( ((181+leap) <= day) && (day < (212+leap)) ) {
    return 6;
  }
  if ( ((212+leap) <= day) && (day < (243+leap)) ) {
    return 7;
  }
  if ( ((243+leap) <= day) && (day < (273+leap)) ) {
    return 8;
  }
  if ( ((273+leap) <= day) && (day < (304+leap)) ) {
    return 9;
  }
  if ( ((304+leap) <= day) && (day < (334+leap)) ) {
    return 10;
  }
  if ( ((334+leap) <= day) && (day < (365+leap)) ) {
    return 11;
  } else {
    return "ERROR: MonthFromTime("+t+") not known";
  }
}
function DayWithinYear( t ) {
  return( Day(t) - DayFromYear(YearFromTime(t)));
}
function DateFromTime( t ) {
  var day = DayWithinYear(t);
  var month = MonthFromTime(t);

  if ( month == 0 ) {
    return ( day + 1 );
  }
  if ( month == 1 ) {
    return ( day - 30 );
  }
  if ( month == 2 ) {
    return ( day - 58 - InLeapYear(t) );
  }
  if ( month == 3 ) {
    return ( day - 89 - InLeapYear(t));
  }
  if ( month == 4 ) {
    return ( day - 119 - InLeapYear(t));
  }
  if ( month == 5 ) {
    return ( day - 150- InLeapYear(t));
  }
  if ( month == 6 ) {
    return ( day - 180- InLeapYear(t));
  }
  if ( month == 7 ) {
    return ( day - 211- InLeapYear(t));
  }
  if ( month == 8 ) {
    return ( day - 242- InLeapYear(t));
  }
  if ( month == 9 ) {
    return ( day - 272- InLeapYear(t));
  }
  if ( month == 10 ) {
    return ( day - 303- InLeapYear(t));
  }
  if ( month == 11 ) {
    return ( day - 333- InLeapYear(t));
  }

  return ("ERROR:  DateFromTime("+t+") not known" );
}
function WeekDay( t ) {
  var weekday = (Day(t)+4) % 7;
  return( weekday < 0 ? 7 + weekday : weekday );
}

// missing daylight savings time adjustment

function HourFromTime( t ) {
  var h = Math.floor( t / msPerHour ) % HoursPerDay;
  return ( (h<0) ? HoursPerDay + h : h  );
}
function MinFromTime( t ) {
  var min = Math.floor( t / msPerMinute ) % MinutesPerHour;
  return( ( min < 0 ) ? MinutesPerHour + min : min  );
}
function SecFromTime( t ) {
  var sec = Math.floor( t / msPerSecond ) % SecondsPerMinute;
  return ( (sec < 0 ) ? SecondsPerMinute + sec : sec );
}
function msFromTime( t ) {
  var ms = t % msPerSecond;
  return ( (ms < 0 ) ? msPerSecond + ms : ms );
}
function LocalTZA() {
  return ( TZ_DIFF * msPerHour );
}
function UTC( t ) {
  return ( t - LocalTZA() - DaylightSavingTA(t - LocalTZA()) );
}

function DaylightSavingTA( t ) {
  t = t - LocalTZA();

  var dst_start = GetFirstSundayInApril(t) + 2*msPerHour;
  var dst_end   = GetLastSundayInOctober(t)+ 2*msPerHour;

  if ( t >= dst_start && t < dst_end ) {
    return msPerHour;
  } else {
    return 0;
  }

  // Daylight Savings Time starts on the first Sunday in April at 2:00AM in
  // PST.  Other time zones will need to override this function.

  print( new Date( UTC(dst_start + LocalTZA())) );

  return UTC(dst_start  + LocalTZA());
}
function GetFirstSundayInApril( t ) {
  var year = YearFromTime(t);
  var leap = InLeapYear(t);

  var april = TimeFromYear(year) + TimeInMonth(0, leap) + TimeInMonth(1,leap) +
    TimeInMonth(2,leap);

  for ( var first_sunday = april; WeekDay(first_sunday) > 0;
        first_sunday += msPerDay )
  {
    ;
  }

  return first_sunday;
}
function GetLastSundayInOctober( t ) {
  var year = YearFromTime(t);
  var leap = InLeapYear(t);

  for ( var oct = TimeFromYear(year), m = 0; m < 9; m++ ) {
    oct += TimeInMonth(m, leap);
  }
  for ( var last_sunday = oct + 30*msPerDay; WeekDay(last_sunday) > 0;
        last_sunday -= msPerDay )
  {
    ;
  }
  return last_sunday;
}
function LocalTime( t ) {
  return ( t + LocalTZA() + DaylightSavingTA(t) );
}
function MakeTime( hour, min, sec, ms ) {
  if ( isNaN( hour ) || isNaN( min ) || isNaN( sec ) || isNaN( ms ) ) {
    return Number.NaN;
  }

  hour = ToInteger(hour);
  min  = ToInteger( min);
  sec  = ToInteger( sec);
  ms  = ToInteger( ms );

  return( (hour*msPerHour) + (min*msPerMinute) +
          (sec*msPerSecond) + ms );
}
function MakeDay( year, month, date ) {
  if ( isNaN(year) || isNaN(month) || isNaN(date) ) {
    return Number.NaN;
  }
  year = ToInteger(year);
  month = ToInteger(month);
  date = ToInteger(date );

  var sign = ( year < 1970 ) ? -1 : 1;
  var t =    ( year < 1970 ) ? 1 :  0;
  var y =    ( year < 1970 ) ? 1969 : 1970;

  var result5 = year + Math.floor( month/12 );
  var result6 = month % 12;

  if ( year < 1970 ) {
    for ( y = 1969; y >= year; y += sign ) {
      t += sign * TimeInYear(y);
    }
  } else {
    for ( y = 1970 ; y < year; y += sign ) {
      t += sign * TimeInYear(y);
    }
  }

  var leap = InLeapYear( t );

  for ( var m = 0; m < month; m++ ) {
    t += TimeInMonth( m, leap );
  }

  if ( YearFromTime(t) != result5 ) {
    return Number.NaN;
  }
  if ( MonthFromTime(t) != result6 ) {
    return Number.NaN;
  }
  if ( DateFromTime(t) != 1 ) {
    return Number.NaN;
  }

  return ( (Day(t)) + date - 1 );
}
function TimeInMonth( month, leap ) {
  // september april june november
  // jan 0  feb 1  mar 2 apr 3 may 4  june 5  jul 6
  // aug 7  sep 8  oct 9 nov 10 dec 11

  if ( month == 3 || month == 5 || month == 8 || month == 10 ) {
    return ( 30*msPerDay );
  }

  // all the rest
  if ( month == 0 || month == 2 || month == 4 || month == 6 ||
       month == 7 || month == 9 || month == 11 ) {
    return ( 31*msPerDay );
  }

  // save february
  return ( (leap == 0) ? 28*msPerDay : 29*msPerDay );
}
function MakeDate( day, time ) {
  if ( day == Number.POSITIVE_INFINITY ||
       day == Number.NEGATIVE_INFINITY ) {
    return Number.NaN;
  }
  if ( time == Number.POSITIVE_INFINITY ||
       time == Number.NEGATIVE_INFINITY ) {
    return Number.NaN;
  }
  return ( day * msPerDay ) + time;
}
function TimeClip( t ) {
  if ( isNaN( t ) ) {
    return ( Number.NaN );
  }
  if ( Math.abs( t ) > 8.64e15 ) {
    return ( Number.NaN );
  }

  return ( ToInteger( t ) );
}
function ToInteger( t ) {
  t = Number( t );

  if ( isNaN( t ) ){
    return ( Number.NaN );
  }
  if ( t == 0 || t == -0 ||
       t == Number.POSITIVE_INFINITY || t == Number.NEGATIVE_INFINITY ) {
    return 0;
  }

  var sign = ( t < 0 ) ? -1 : 1;

  return ( sign * Math.floor( Math.abs( t ) ) );
}
function Enumerate ( o ) {
  var p;
  for ( p in o ) {
    print( p +": " + o[p] );
  }
}

/* these functions are useful for running tests manually in Rhino */

function GetContext() {
  return Packages.com.netscape.javascript.Context.getCurrentContext();
}
function OptLevel( i ) {
  i = Number(i);
  var cx = GetContext();
  cx.setOptimizationLevel(i);
}
/* end of Rhino functions */


/* JavaScriptOptions
   encapsulate the logic for setting and retrieving the values
   of the javascript options.
   
   Note: in shell, options() takes an optional comma delimited list
   of option names, toggles the values for each option and returns the
   list of option names which were set before the call. 
   If no argument is passed to options(), it returns the current
   options with value true.

   Usage;

   // create and initialize object.
   jsOptions = new JavaScriptOptions();

   // set a particular option
   jsOptions.setOption(name, boolean);

   // reset all options to their original values.
   jsOptions.reset();
*/

function JavaScriptOptions()
{
  this.orig   = {};
  this.orig.strict = this.strict = false;
  this.orig.werror = this.werror = false;

  this.privileges = 'UniversalXPConnect UniversalPreferencesRead ' + 
                    'UniversalPreferencesWrite';

  if (typeof options == 'function')
  {
    // shell
    var optString = options();
    if (optString)
    {
      var optList = optString.split(',');
      for (var iOpt = 0; iOpt < optList.length; iOpt++)
      {
        optName = optList[iOpt];
        this[optName] = true;
      }
    }
  }
  else if (typeof netscape != 'undefined' && 'security' in netscape)
  {
    // browser
    netscape.security.PrivilegeManager.enablePrivilege(this.privileges);

    var preferences = Components.classes['@mozilla.org/preferences-service;1'];
    if (!preferences)
    {
      throw 'JavaScriptOptions: unable to get @mozilla.org/preferences-service;1';
    }

    var prefService = preferences.
      getService(Components.interfaces.nsIPrefService);

    if (!prefService)
    {
      throw 'JavaScriptOptions: unable to get nsIPrefService';
    }

    var pref = prefService.getBranch('');

    if (!pref)
    {
      throw 'JavaScriptOptions: unable to get prefService branch';
    }

    try
    {
      this.orig.strict = this.strict = 
        pref.getBoolPref('javascript.options.strict');
    }
    catch(e)
    {
    }

    try
    {
      this.orig.werror = this.werror = 
        pref.getBoolPref('javascript.options.werror');
    }
    catch(e)
    {
    }
  }
}

JavaScriptOptions.prototype.setOption = 
function (optionName, optionValue)
{
  if (typeof options == 'function')
  {
    // shell
    if (this[optionName] != optionValue)
    {
      options(optionName);
    }
  }
  else if (typeof netscape != 'undefined' && 'security' in netscape)
  {
    // browser
    netscape.security.PrivilegeManager.enablePrivilege(this.privileges);

    var preferences = Components.classes['@mozilla.org/preferences-service;1'];
    if (!preferences)
    {
      throw 'setOption: unable to get @mozilla.org/preferences-service;1';
    }

    var prefService = preferences.
    getService(Components.interfaces.nsIPrefService);

    if (!prefService)
    {
      throw 'setOption: unable to get nsIPrefService';
    }

    var pref = prefService.getBranch('');

    if (!pref)
    {
      throw 'setOption: unable to get prefService branch';
    }

    pref.setBoolPref('javascript.options.' + optionName, optionValue);
  }

  this[optionName] = optionValue;

  return;
}


JavaScriptOptions.prototype.reset = function ()
{
  this.setOption('strict', this.orig.strict);
  this.setOption('werror', this.orig.werror);
}


