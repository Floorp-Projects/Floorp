/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var SECTION = "15.9.4.3";
var TITLE = "Date.UTC( year, month, date, hours, minutes, seconds, ms )";

// Dates around 1970

addNewTestCase( Date.UTC( 1970,0,1,0,0,0,0),
		"Date.UTC( 1970,0,1,0,0,0,0)",
		utc(1970,0,1,0,0,0,0) );

addNewTestCase( Date.UTC( 1969,11,31,23,59,59,999),
		"Date.UTC( 1969,11,31,23,59,59,999)",
		utc(1969,11,31,23,59,59,999) );
addNewTestCase( Date.UTC( 1972,1,29,23,59,59,999),
		"Date.UTC( 1972,1,29,23,59,59,999)",
		utc(1972,1,29,23,59,59,999) );
addNewTestCase( Date.UTC( 1972,2,1,23,59,59,999),
		"Date.UTC( 1972,2,1,23,59,59,999)",
		utc(1972,2,1,23,59,59,999) );
addNewTestCase( Date.UTC( 1968,1,29,23,59,59,999),
		"Date.UTC( 1968,1,29,23,59,59,999)",
		utc(1968,1,29,23,59,59,999) );
addNewTestCase( Date.UTC( 1968,2,1,23,59,59,999),
		"Date.UTC( 1968,2,1,23,59,59,999)",
		utc(1968,2,1,23,59,59,999) );
addNewTestCase( Date.UTC( 1969,0,1,0,0,0,0),
		"Date.UTC( 1969,0,1,0,0,0,0)",
		utc(1969,0,1,0,0,0,0) );
addNewTestCase( Date.UTC( 1969,11,31,23,59,59,1000),
		"Date.UTC( 1969,11,31,23,59,59,1000)",
		utc(1970,0,1,0,0,0,0) );
addNewTestCase( Date.UTC( 1969,Number.NaN,31,23,59,59,999),
		"Date.UTC( 1969,Number.NaN,31,23,59,59,999)",
		utc(1969,Number.NaN,31,23,59,59,999) );

// Dates around 2000

addNewTestCase( Date.UTC( 1999,11,31,23,59,59,999),
		"Date.UTC( 1999,11,31,23,59,59,999)",
		utc(1999,11,31,23,59,59,999) );
addNewTestCase( Date.UTC( 2000,0,1,0,0,0,0),
		"Date.UTC( 2000,0,1,0,0,0,0)",
		utc(2000,0,1,0,0,0,0) );

// Dates around 1900
addNewTestCase( Date.UTC( 1899,11,31,23,59,59,999),
		"Date.UTC( 1899,11,31,23,59,59,999)",
		utc(1899,11,31,23,59,59,999) );
addNewTestCase( Date.UTC( 1900,0,1,0,0,0,0),
		"Date.UTC( 1900,0,1,0,0,0,0)",
		utc(1900,0,1,0,0,0,0) );
addNewTestCase( Date.UTC( 1973,0,1,0,0,0,0),
		"Date.UTC( 1973,0,1,0,0,0,0)",
		utc(1973,0,1,0,0,0,0) );
addNewTestCase( Date.UTC( 1776,6,4,12,36,13,111),
		"Date.UTC( 1776,6,4,12,36,13,111)",
		utc(1776,6,4,12,36,13,111) );
addNewTestCase( Date.UTC( 2525,9,18,15,30,1,123),
		"Date.UTC( 2525,9,18,15,30,1,123)",
		utc(2525,9,18,15,30,1,123) );

// Dates around 29 Feb 2000

addNewTestCase( Date.UTC( 2000,1,29,0,0,0,0 ),
		"Date.UTC( 2000,1,29,0,0,0,0 )",
		utc(2000,1,29,0,0,0,0) );
addNewTestCase( Date.UTC( 2000,1,29,8,0,0,0 ),
		"Date.UTC( 2000,1,29,8,0,0,0 )",
		utc(2000,1,29,8,0,0,0) );

// Dates around 1 Jan 2005

addNewTestCase( Date.UTC( 2005,0,1,0,0,0,0 ),
		"Date.UTC( 2005,0,1,0,0,0,0 )",
		utc(2005,0,1,0,0,0,0) );
addNewTestCase( Date.UTC( 2004,11,31,16,0,0,0 ),
		"Date.UTC( 2004,11,31,16,0,0,0 )",
		utc(2004,11,31,16,0,0,0) );

test();

function addNewTestCase( DateCase, DateString, ExpectDate) {
  DateCase = DateCase;

  new TestCase( SECTION, DateString,         ExpectDate.value,       DateCase );
  new TestCase( SECTION, DateString,         ExpectDate.value,       DateCase );
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

function utc( year, month, date, hours, minutes, seconds, ms ) {
  d = new MyDate();
  d.year      = Number(year);

  if (month)
    d.month     = Number(month);
  if (date)
    d.date      = Number(date);
  if (hours)
    d.hours     = Number(hours);
  if (minutes)
    d.minutes   = Number(minutes);
  if (seconds)
    d.seconds   = Number(seconds);
  if (ms)
    d.ms        = Number(ms);

  if ( isNaN(d.year) && 0 <= ToInteger(d.year) && d.year <= 99 ) {
    d.year = 1900 + ToInteger(d.year);
  }

  if (isNaN(month) || isNaN(year) || isNaN(date) || isNaN(hours) ||
      isNaN(minutes) || isNaN(seconds) || isNaN(ms) ) {
    d.year = Number.NaN;
    d.month = Number.NaN;
    d.date = Number.NaN;
    d.hours = Number.NaN;
    d.minutes = Number.NaN;
    d.seconds = Number.NaN;
    d.ms = Number.NaN;
    d.value = Number.NaN;
    d.time = Number.NaN;
    d.day =Number.NaN;
    return d;
  }

  d.day = MakeDay( d.year, d.month, d.date );
  d.time = MakeTime( d.hours, d.minutes, d.seconds, d.ms );
  d.value = (TimeClip( MakeDate(d.day,d.time)));

  return d;
}

function UTCTime( t ) {
  sign = ( t < 0 ) ? -1 : 1;
  return ( (t +(TZ_DIFF*msPerHour)) );
}

