/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
   File Name:    15.9.5.7.js
   ECMA Section: 15.9.5.7 Date.prototype.toLocaleTimeString()
   Description:
   This function returns a string value. The contents of the string are
   implementation dependent, but are intended to represent the "time"
   portion of the Date in the current time zone in a convenient,
   human-readable form.   We test the content of the string by checking
   that

   new Date(d.toDateString() + " " + d.toLocaleTimeString()) ==  d

   Author:  pschwartau@netscape.com                            
   Date:    14 november 2000
   Revised: 07 january 2002  because of a change in JS Date format:
   Revised: 21 November 2005 since the string comparison stuff is horked.
   bclary

   See http://bugzilla.mozilla.org/show_bug.cgi?id=118266 (SpiderMonkey)
   See http://bugzilla.mozilla.org/show_bug.cgi?id=118636 (Rhino)
*/
//-----------------------------------------------------------------------------
var SECTION = "15.9.5.7";
var TITLE   = "Date.prototype.toLocaleTimeString()";
  
var status = '';
var actual = ''; 
var expect = '';
var givenDate;
var year = '';
var regexp = '';
var TimeString = '';
var reducedDateString = '';
var hopeThisIsLocaleTimeString = '';
var cnERR ='OOPS! FATAL ERROR: no regexp match in extractLocaleTimeString()';

writeHeaderToLog( SECTION + " "+ TITLE);

// first, a couple generic tests -

status = "typeof (now.toLocaleTimeString())"; 
actual =   typeof (now.toLocaleTimeString());
expect = "string";
addTestCase();

status = "Date.prototype.toLocaleTimeString.length";  
actual =  Date.prototype.toLocaleTimeString.length;
expect =  0;  
addTestCase();

// 1970
addDateTestCase(0);
addDateTestCase(TZ_ADJUST);
  
// 1900
addDateTestCase(TIME_1900);
addDateTestCase(TIME_1900 - TZ_ADJUST);

// 2000
addDateTestCase(TIME_2000);
addDateTestCase(TIME_2000 - TZ_ADJUST);
   
// 29 Feb 2000
addDateTestCase(UTC_29_FEB_2000);
addDateTestCase(UTC_29_FEB_2000 - 1000);
addDateTestCase(UTC_29_FEB_2000 - TZ_ADJUST);

// Now
addDateTestCase( TIME_NOW);
addDateTestCase( TIME_NOW - TZ_ADJUST);

// 2005
addDateTestCase(UTC_1_JAN_2005);
addDateTestCase(UTC_1_JAN_2005 - 1000);
addDateTestCase(UTC_1_JAN_2005 - TZ_ADJUST);

test();

function addTestCase()
{
  new TestCase(
    status,
    expect,
    actual);
}


function addDateTestCase(date_given_in_milliseconds)
{
  var s = 'new Date(' +  date_given_in_milliseconds + ')';
  givenDate = new Date(date_given_in_milliseconds);
  
  status = 'd = ' + s +
    '; d == new Date(d.toDateString() + " " + d.toLocaleTimeString())';  
  expect = givenDate.toString();
  actual = new Date(givenDate.toDateString() +
                    ' ' + givenDate.toLocaleTimeString()).toString();
  addTestCase();
}

