// |reftest| random-if(Android)
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.9.5.35-1.js
   ECMA Section:       15.9.5.35 Date.prototype.setUTCMonth(mon [,date])
   Description:
   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "15.9.5.35-1";
var VERSION = "ECMA_1";
startTest();

writeHeaderToLog( SECTION + " Date.prototype.setUTCMonth(mon [,date] ) ");
addNewTestCase( "TDATE = new Date(0);(TDATE).setUTCMonth(0);TDATE",
                UTCDateFromTime(SetUTCMonth(0,0)),
                LocalDateFromTime(SetUTCMonth(0,0)) );

addNewTestCase( "TDATE = new Date(0);(TDATE).setUTCMonth(11);TDATE",
                UTCDateFromTime(SetUTCMonth(0,11)),
                LocalDateFromTime(SetUTCMonth(0,11)) );

addNewTestCase( "TDATE = new Date(0);(TDATE).setUTCMonth(5,4);TDATE",
                UTCDateFromTime(SetUTCMonth(0,5,4)),
                LocalDateFromTime(SetUTCMonth(0,5,4)) );

test();

function addNewTestCase( DateString, UTCDate, LocalDate) {
  DateCase = eval( DateString );

  new TestCase( SECTION, DateString+".getTime()",             UTCDate.value,       DateCase.getTime() );
  new TestCase( SECTION, DateString+".valueOf()",             UTCDate.value,       DateCase.valueOf() );

  new TestCase( SECTION, DateString+".getUTCFullYear()",      UTCDate.year,        DateCase.getUTCFullYear() );
  new TestCase( SECTION, DateString+".getUTCMonth()",         UTCDate.month,       DateCase.getUTCMonth() );
  new TestCase( SECTION, DateString+".getUTCDate()",          UTCDate.date,        DateCase.getUTCDate() );
  new TestCase( SECTION, DateString+".getUTCDay()",           UTCDate.day,         DateCase.getUTCDay() );
  new TestCase( SECTION, DateString+".getUTCHours()",         UTCDate.hours,       DateCase.getUTCHours() );
  new TestCase( SECTION, DateString+".getUTCMinutes()",       UTCDate.minutes,     DateCase.getUTCMinutes() );
  new TestCase( SECTION, DateString+".getUTCSeconds()",       UTCDate.seconds,     DateCase.getUTCSeconds() );
  new TestCase( SECTION, DateString+".getUTCMilliseconds()",  UTCDate.ms,          DateCase.getUTCMilliseconds() );
   
  new TestCase( SECTION, DateString+".getFullYear()",         LocalDate.year,       DateCase.getFullYear() );
  new TestCase( SECTION, DateString+".getMonth()",            LocalDate.month,      DateCase.getMonth() );
  new TestCase( SECTION, DateString+".getDate()",             LocalDate.date,       DateCase.getDate() );
  new TestCase( SECTION, DateString+".getDay()",              LocalDate.day,        DateCase.getDay() );
  new TestCase( SECTION, DateString+".getHours()",            LocalDate.hours,      DateCase.getHours() );
  new TestCase( SECTION, DateString+".getMinutes()",          LocalDate.minutes,    DateCase.getMinutes() );
  new TestCase( SECTION, DateString+".getSeconds()",          LocalDate.seconds,    DateCase.getSeconds() );
  new TestCase( SECTION, DateString+".getMilliseconds()",     LocalDate.ms,         DateCase.getMilliseconds() );

  DateCase.toString = Object.prototype.toString;

  new TestCase( SECTION,
                DateString+".toString=Object.prototype.toString;"+DateString+".toString()",
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
function SetUTCMonth( t, month, date ) {
  var T       = t;
  var MONTH   = Number( month );
  var DATE    = ( date == void 0) ? DateFromTime(T) : Number( date );

  var RESULT4 = MakeDay(YearFromTime(T), MONTH, DATE );
  var RESULT5 = MakeDate( RESULT4, TimeWithinDay(T));

  return ( TimeClip(RESULT5) );
}
