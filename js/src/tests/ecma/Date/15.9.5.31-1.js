/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.9.5.31-1.js

   ECMA Section:      
   15.9.5.31 Date.prototype.setUTCHours(hour [, min [, sec [, ms ]]] )

   Description:

   If min is not specified, this behaves as if min were specified with
   the value getUTCMinutes( ).  If sec is not specified, this behaves
   as if sec were specified with the value getUTCSeconds ( ).  If ms
   is not specified, this behaves as if ms were specified with the
   value getUTCMilliseconds( ).

   1.Let t be this time value.
   2.Call ToNumber(hour).
   3.If min is not specified, compute MinFromTime(t);
   otherwise, call ToNumber(min).
   4.If sec is not specified, compute SecFromTime(t);
   otherwise, call ToNumber(sec).
   5.If ms is not specified, compute msFromTime(t);
   otherwise, call ToNumber(ms).
   6.Compute MakeTime(Result(2), Result(3), Result(4), Result(5)).
   7.Compute MakeDate(Day(t), Result(6)).
   8.Set the [[Value]] property of the this value to TimeClip(Result(7)).

   1.Return the value of the [[Value]] property of the this value.
   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "15.9.5.31-1";
var VERSION = "ECMA_1";
startTest();

writeHeaderToLog(SECTION +
                 " Date.prototype.setUTCHours(hour [, min [, sec [, ms ]]] )");

addNewTestCase( 0, 0, void 0, void 0, void 0,
                "TDATE = new Date(0);(TDATE).setUTCHours(0);TDATE",
                UTCDateFromTime(SetUTCHours(0,0,0,0)),
                LocalDateFromTime(SetUTCHours(0,0,0,0)) );

addNewTestCase( 28800000, 23, 59, 999, void 0,
                "TDATE = new Date(28800000);(TDATE).setUTCHours(23,59,999);TDATE",
                UTCDateFromTime(SetUTCHours(28800000,23,59,999)),
                LocalDateFromTime(SetUTCHours(28800000,23,59,999)) );

addNewTestCase( 28800000,999,999, void 0, void 0,
                "TDATE = new Date(28800000);(TDATE).setUTCHours(999,999);TDATE",
                UTCDateFromTime(SetUTCHours(28800000,999,999)),
                LocalDateFromTime(SetUTCHours(28800000,999,999)) );

addNewTestCase( 28800000, 999, void 0, void 0, void 0,
                "TDATE = new Date(28800000);(TDATE).setUTCHours(999);TDATE",
                UTCDateFromTime(SetUTCHours(28800000,999,0)),
                LocalDateFromTime(SetUTCHours(28800000,999,0)) );

addNewTestCase( 28800000, -8670, void 0, void 0, void 0,
                "TDATE = new Date(28800000);(TDATE).setUTCHours(-8670);TDATE",
                UTCDateFromTime(SetUTCHours(28800000,-8670)),
                LocalDateFromTime(SetUTCHours(28800000,-8670)) );

// modify hours to remove dst ambiguity
addNewTestCase( 946684800000, 1235567, void 0, void 0, void 0,
                "TDATE = new Date(946684800000);(TDATE).setUTCHours(1235567);TDATE",
                UTCDateFromTime(SetUTCHours(946684800000,1235567)),
                LocalDateFromTime(SetUTCHours(946684800000,1235567)) );

addNewTestCase( -2208988800000, 59, 999, void 0, void 0,
                "TDATE = new Date(-2208988800000);(TDATE).setUTCHours(59,999);TDATE",
                UTCDateFromTime(SetUTCHours(-2208988800000,59,999)),
                LocalDateFromTime(SetUTCHours(-2208988800000,59,999)) );

test();

function addNewTestCase( time, hours, min, sec, ms, DateString, UTCDate, LocalDate) {

  DateCase = new Date(time);
  if ( min == void 0 ) {
    DateCase.setUTCHours( hours );
  } else {
    if ( sec == void 0 ) {
      DateCase.setUTCHours( hours, min );
    } else {
      if ( ms == void 0 ) {
        DateCase.setUTCHours( hours, min, sec );
      } else {
        DateCase.setUTCHours( hours, min, sec, ms );
      }
    }
  }

  new TestCase( SECTION, DateString+".getTime()",            UTCDate.value,
                DateCase.getTime() );
  new TestCase( SECTION, DateString+".valueOf()",            UTCDate.value,
                DateCase.valueOf() );
  new TestCase( SECTION, DateString+".getUTCFullYear()",     UTCDate.year,
                DateCase.getUTCFullYear() );
  new TestCase( SECTION, DateString+".getUTCMonth()",        UTCDate.month,
                DateCase.getUTCMonth() );
  new TestCase( SECTION, DateString+".getUTCDate()",         UTCDate.date,
                DateCase.getUTCDate() );
  new TestCase( SECTION, DateString+".getUTCDay()",          UTCDate.day,
                DateCase.getUTCDay() );
  new TestCase( SECTION, DateString+".getUTCHours()",        UTCDate.hours,
                DateCase.getUTCHours() );
  new TestCase( SECTION, DateString+".getUTCMinutes()",      UTCDate.minutes,
                DateCase.getUTCMinutes() );
  new TestCase( SECTION, DateString+".getUTCSeconds()",      UTCDate.seconds,
                DateCase.getUTCSeconds() );
  new TestCase( SECTION, DateString+".getUTCMilliseconds()", UTCDate.ms,
                DateCase.getUTCMilliseconds() );

  new TestCase( SECTION, DateString+".getFullYear()",        LocalDate.year,
                DateCase.getFullYear() );
  new TestCase( SECTION, DateString+".getMonth()",           LocalDate.month,
                DateCase.getMonth() );
  new TestCase( SECTION, DateString+".getDate()",            LocalDate.date,
                DateCase.getDate() );
  new TestCase( SECTION, DateString+".getDay()",             LocalDate.day,
                DateCase.getDay() );
  new TestCase( SECTION, DateString+".getHours()",           LocalDate.hours,
                DateCase.getHours() );
  new TestCase( SECTION, DateString+".getMinutes()",         LocalDate.minutes,
                DateCase.getMinutes() );
  new TestCase( SECTION, DateString+".getSeconds()",         LocalDate.seconds,
                DateCase.getSeconds() );
  new TestCase( SECTION, DateString+".getMilliseconds()",    LocalDate.ms,
                DateCase.getMilliseconds() );

  DateCase.toString = Object.prototype.toString;

  new TestCase( SECTION,
                DateString+".toString=Object.prototype.toString;" +
                DateString+".toString()",
                "[object Date]",
                DateCase.toString() );
}
function MyDate() {
  this.year = 0;
  this.month = 0;
  this.date = 0;
  this.hours = 0;
  this.minutes = 0;
  this.seconds = 0;
  this.ms = 0;
}
function LocalDateFromTime(t) {
  t = LocalTime(t);
  return ( MyDateFromTime(t) );
}
function UTCDateFromTime(t) {
  return ( MyDateFromTime(t) );
}
function MyDateFromTime( t ) {
  var d = new MyDate();
  d.year = YearFromTime(t);
  d.month = MonthFromTime(t);
  d.date = DateFromTime(t);
  d.hours = HourFromTime(t);
  d.minutes = MinFromTime(t);
  d.seconds = SecFromTime(t);
  d.ms = msFromTime(t);

  d.time = MakeTime( d.hours, d.minutes, d.seconds, d.ms );
  d.value = TimeClip( MakeDate( MakeDay( d.year, d.month, d.date ), d.time ) );
  d.day = WeekDay( d.value );

  return (d);
}
function SetUTCHours( t, hour, min, sec, ms ) {
  var TIME = t;
  var HOUR = Number(hour);
  var MIN =  ( min == void 0) ? MinFromTime(TIME) : Number(min);
  var SEC  = ( sec == void 0) ? SecFromTime(TIME) : Number(sec);
  var MS   = ( ms == void 0 ) ? msFromTime(TIME)  : Number(ms);
  var RESULT6 = MakeTime( HOUR,
                          MIN,
                          SEC,
                          MS );
  return ( TimeClip(MakeDate(Day(TIME), RESULT6)) );
}
