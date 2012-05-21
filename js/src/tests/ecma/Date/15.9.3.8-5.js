/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.9.3.8.js
   ECMA Section:       15.9.3.8 The Date Constructor
   new Date( value )
   Description:        The [[Prototype]] property of the newly constructed
   object is set to the original Date prototype object,
   the one that is the initial valiue of Date.prototype.

   The [[Class]] property of the newly constructed object is
   set to "Date".

   The [[Value]] property of the newly constructed object is
   set as follows:

   1. Call ToPrimitive(value)
   2. If Type( Result(1) ) is String, then go to step 5.
   3. Let V be  ToNumber( Result(1) ).
   4. Set the [[Value]] property of the newly constructed
   object to TimeClip(V) and return.
   5. Parse Result(1) as a date, in exactly the same manner
   as for the parse method.  Let V be the time value for
   this date.
   6. Go to step 4.

   Author:             christine@netscape.com
   Date:               28 october 1997
   Version:            9706

*/

var VERSION = "ECMA_1";
startTest();
var SECTION = "15.9.3.8";
var TYPEOF  = "object";

var TIME        = 0;
var UTC_YEAR    = 1;
var UTC_MONTH   = 2;
var UTC_DATE    = 3;
var UTC_DAY     = 4;
var UTC_HOURS   = 5;
var UTC_MINUTES = 6;
var UTC_SECONDS = 7;
var UTC_MS      = 8;

var YEAR        = 9;
var MONTH       = 10;
var DATE        = 11;
var DAY         = 12;
var HOURS       = 13;
var MINUTES     = 14;
var SECONDS     = 15;
var MS          = 16;


//  for TCMS, the gTestcases array must be global.
var gTc= 0;
var TITLE = "Date constructor:  new Date( value )";
var SECTION = "15.9.3.8";
var VERSION = "ECMA_1";
startTest();

writeHeaderToLog( SECTION +" " + TITLE );

// all the "ResultArrays" below are hard-coded to Pacific Standard Time values -
var TZ_ADJUST =  -TZ_PST * msPerHour;


// Dates around 1900

var PST_1900 = TIME_1900 + 8*msPerHour;

addNewTestCase( new Date( TIME_1900 ),
		"new Date("+TIME_1900+")",
		[TIME_1900,1900,0,1,1,0,0,0,0,1899,11,31,0,16,0,0,0] );

addNewTestCase( new Date(PST_1900),
		"new Date("+PST_1900+")",
		[ PST_1900,1900,0,1,1,8,0,0,0,1900,0,1,1,0,0,0,0] );

addNewTestCase( new Date( (new Date(TIME_1900)).toString() ),
		"new Date(\""+(new Date(TIME_1900)).toString()+"\")",
		[TIME_1900,1900,0,1,1,0,0,0,0,1899,11,31,0,16,0,0,0] );

addNewTestCase( new Date( (new Date(PST_1900)).toString() ),
		"new Date(\""+(new Date(PST_1900 )).toString()+"\")",
		[ PST_1900,1900,0,1,1,8,0,0,0,1900,0,1,1,0,0,0,0] );

addNewTestCase( new Date( (new Date(TIME_1900)).toUTCString() ),
		"new Date(\""+(new Date(TIME_1900)).toUTCString()+"\")",
		[TIME_1900,1900,0,1,1,0,0,0,0,1899,11,31,0,16,0,0,0] );

addNewTestCase( new Date( (new Date(PST_1900)).toUTCString() ),
		"new Date(\""+(new Date(PST_1900 )).toUTCString()+"\")",
		[ PST_1900,1900,0,1,1,8,0,0,0,1900,0,1,1,0,0,0,0] );

test();

function addNewTestCase( DateCase, DateString, ResultArray ) {
  //adjust hard-coded ResultArray for tester's timezone instead of PST
  adjustResultArray(ResultArray, 'msMode');

  new TestCase( SECTION, DateString+".getTime()", ResultArray[TIME],       DateCase.getTime() );
  new TestCase( SECTION, DateString+".valueOf()", ResultArray[TIME],       DateCase.valueOf() );
  new TestCase( SECTION, DateString+".getUTCFullYear()",      ResultArray[UTC_YEAR], DateCase.getUTCFullYear() );
  new TestCase( SECTION, DateString+".getUTCMonth()",         ResultArray[UTC_MONTH],  DateCase.getUTCMonth() );
  new TestCase( SECTION, DateString+".getUTCDate()",          ResultArray[UTC_DATE],   DateCase.getUTCDate() );
  new TestCase( SECTION, DateString+".getUTCDay()",           ResultArray[UTC_DAY],    DateCase.getUTCDay() );
  new TestCase( SECTION, DateString+".getUTCHours()",         ResultArray[UTC_HOURS],  DateCase.getUTCHours() );
  new TestCase( SECTION, DateString+".getUTCMinutes()",       ResultArray[UTC_MINUTES],DateCase.getUTCMinutes() );
  new TestCase( SECTION, DateString+".getUTCSeconds()",       ResultArray[UTC_SECONDS],DateCase.getUTCSeconds() );
  new TestCase( SECTION, DateString+".getUTCMilliseconds()",  ResultArray[UTC_MS],     DateCase.getUTCMilliseconds() );
  new TestCase( SECTION, DateString+".getFullYear()",         ResultArray[YEAR],       DateCase.getFullYear() );
  new TestCase( SECTION, DateString+".getMonth()",            ResultArray[MONTH],      DateCase.getMonth() );
  new TestCase( SECTION, DateString+".getDate()",             ResultArray[DATE],       DateCase.getDate() );
  new TestCase( SECTION, DateString+".getDay()",              ResultArray[DAY],        DateCase.getDay() );
  new TestCase( SECTION, DateString+".getHours()",            ResultArray[HOURS],      DateCase.getHours() );
  new TestCase( SECTION, DateString+".getMinutes()",          ResultArray[MINUTES],    DateCase.getMinutes() );
  new TestCase( SECTION, DateString+".getSeconds()",          ResultArray[SECONDS],    DateCase.getSeconds() );
  new TestCase( SECTION, DateString+".getMilliseconds()",     ResultArray[MS],         DateCase.getMilliseconds() );
}
