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

    var VERSION = "ECMA_1";
    startTest();
    var SECTION = "15.9.4.2";
    var TITLE   = "Date.parse()";

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
    var TYPEOF  = "object";

//  for TCMS, the testcases array must be global.
    writeHeaderToLog("15.9.4.2 Date.parse()" );
    var tc= 0;
    testcases = getTestCases();

//  all tests must call a function that returns an array of TestCase objects.
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    // Dates around 1970

    addNewTestCase( new Date(0),
                    "new Date(0)",
                    [0,1970,0,1,4,0,0,0,0,1969,11,31,3,16,0,0,0] );

    addNewTestCase( new Date(-1),
                    "new Date(-1)",
                    [-1,1969,11,31,3,23,59,59,999,1969,11,31,3,15,59,59,999] );
    addNewTestCase( new Date(28799999),
                    "new Date(28799999)",
                    [28799999,1970,0,1,4,7,59,59,999,1969,11,31,3,23,59,59,999] );
    addNewTestCase( new Date(28800000),
                    "new Date(28800000)",
                    [28800000,1970,0,1,4,8,0,0,0,1970,0,1,4,0,0,0,0] );

    // Dates around 2000

    addNewTestCase( new Date(946684799999),
                    "new Date(946684799999)",
                    [946684799999,1999,11,31,5,23,59,59,999,1999,11,31,5,15,59,59,999] );
    addNewTestCase( new Date(946713599999),
                    "new Date(946713599999)",
                    [946713599999,2000,0,1,6,7,59,59,999,1999,11,31,5,23,59,59,999] );
    addNewTestCase( new Date(946684800000),
                    "new Date(946684800000)",
                    [946684800000,2000,0,1,6,0,0,0,0,1999,11,31,5, 16,0,0,0] );
    addNewTestCase( new Date(946713600000),
                    "new Date(946713600000)",
                    [946713600000,2000,0,1,6,8,0,0,0,2000,0,1,6,0,0,0,0] );

    // Dates around 1900

    addNewTestCase( new Date(-2208988800000),
                    "new Date(-2208988800000)",
                    [-2208988800000,1900,0,1,1,0,0,0,0,1899,11,31,0,16,0,0,0] );

    addNewTestCase( new Date(-2208988800001),
                    "new Date(-2208988800001)",
                    [-2208988800001,1899,11,31,0,23,59,59,999,1899,11,31,0,15,59,59,999] );

    addNewTestCase( new Date(-2208960000001),
                    "new Date(-2208960000001)",
                    [-2208960000001,1900,0,1,1,7,59,59,999,1899,11,31,0,23,59,59,999] );
    addNewTestCase( new Date(-2208960000000),
                    "new Date(-2208960000000)",
                    [-2208960000000,1900,0,1,1,8,0,0,0,1900,0,1,1,0,0,0,0] );
    addNewTestCase( new Date(-2208959999999),
                    "new Date(-2208959999999)",
                    [-2208959999999,1900,0,1,1,8,0,0,1,1900,0,1,1,0,0,0,1] );

    // Dates around Feb 29, 2000

    var UTC_FEB_29_2000 = TIME_2000 + 31*msPerDay + 28*msPerDay;
    var PST_FEB_29_2000 = UTC_FEB_29_2000 + 8*msPerHour;
    addNewTestCase( new Date(UTC_FEB_29_2000),
                    "new Date(" + UTC_FEB_29_2000 +")",
                    [UTC_FEB_29_2000,2000,1,29,2,0,0,0,0,2000,1,28,1,16,0,0,0] );
    addNewTestCase( new Date(PST_FEB_29_2000),
                    "new Date(" + PST_FEB_29_2000 +")",
                    [PST_FEB_29_2000,2000,1,29,2,8,0,0,0,2000,1,29,2,0,0,0,0]);

    // Dates around Jan 1 2005

    var UTC_JAN_1_2005 = TIME_2000 + TimeInYear(2000) + TimeInYear(2001) +
    TimeInYear(2002) + TimeInYear(2003) + TimeInYear(2004);
    var PST_JAN_1_2005 = UTC_JAN_1_2005 + 8*msPerHour;

    addNewTestCase( new Date(UTC_JAN_1_2005),
                    "new Date("+ UTC_JAN_1_2005 +")",
                    [UTC_JAN_1_2005,2005,0,1,6,0,0,0,0,2004,11,31,5,16,0,0,0] );
    addNewTestCase( new Date(PST_JAN_1_2005),
                    "new Date("+ PST_JAN_1_2005 +")",
                    [PST_JAN_1_2005,2005,0,1,6,8,0,0,0,2005,0,1,6,0,0,0,0] );



	function addNewTestCase( DateCase, DateString, ResultArray ) {
	    DateCase = DateCase;
	    adjustResultArray(ResultArray, 'msMode');
	    item = array.length;

	    array[item++] = new TestCase( SECTION, DateString+".getTime()", ResultArray[TIME],       DateCase.getTime() );
	    array[item++] = new TestCase( SECTION, DateString+".valueOf()", ResultArray[TIME],       DateCase.valueOf() );
	    array[item++] = new TestCase( SECTION, "Date.parse( + DateCase.toString() +)",    Math.floor(ResultArray[TIME]/1000)*1000,  Date.parse(DateCase.toString()) );

	    var mydate = new Date(0,0,0,0,0,0,0);
	    array[item++] = new TestCase( SECTION, "mydate.valueOf()==Date.parse(mydate.toString())", true,       mydate.valueOf()==Date.parse(mydate.toString()) );

	    array[item++] = new TestCase( SECTION, "mydate.valueOf()==Date.parse(mydate.toUTCString())", true,       mydate.valueOf()==Date.parse(mydate.toUTCString()) );

	    array[item++] = new TestCase( SECTION, "Date.parse(mydate.toString())==Date.parse(mydate.toUTCString())", true,       Date.parse(mydate.toString())==Date.parse(mydate.toUTCString()) );


	 //    array[item++] = new TestCase( SECTION, "Date.parse(" + DateCase.toGMTString() +")", Math.floor(ResultArray[TIME]/1000)*1000,  Date.parse(DateCase.toGMTString()) );

	// Adding the test case vector for toUTCString instead of the toGMTString, if it is required later on.
	    array[item++] = new TestCase( SECTION, "Date.parse(" + DateCase.toUTCString() +")", Math.floor(ResultArray[TIME]/1000)*1000,  Date.parse(DateCase.toUTCString()) );
	    array[item++] = new TestCase( SECTION, DateString+".getUTCFullYear()",        ResultArray[UTC_YEAR],   DateCase.getUTCFullYear() );
	    array[item++] = new TestCase( SECTION, DateString+".getUTCMonth()",         ResultArray[UTC_MONTH],  DateCase.getUTCMonth() );
	    array[item++] = new TestCase( SECTION, DateString+".getUTCDate()",          ResultArray[UTC_DATE],   DateCase.getUTCDate() );
	    array[item++] = new TestCase( SECTION, DateString+".getUTCDay()",           ResultArray[UTC_DAY],    DateCase.getUTCDay() );
	    array[item++] = new TestCase( SECTION, DateString+".getUTCHours()",         ResultArray[UTC_HOURS],  DateCase.getUTCHours() );
	    array[item++] = new TestCase( SECTION, DateString+".getUTCMinutes()",       ResultArray[UTC_MINUTES],DateCase.getUTCMinutes() );
	    array[item++] = new TestCase( SECTION, DateString+".getUTCSeconds()",       ResultArray[UTC_SECONDS],DateCase.getUTCSeconds() );
	    array[item++] = new TestCase( SECTION, DateString+".getUTCMilliseconds()",  ResultArray[UTC_MS],     DateCase.getUTCMilliseconds() );



	    array[item++] = new TestCase( SECTION, DateString+".getFullYear()",         ResultArray[YEAR],       DateCase.getFullYear() );

	    array[item++] = new TestCase( SECTION, DateString+".getMonth()",            ResultArray[MONTH],      DateCase.getMonth() );
	    array[item++] = new TestCase( SECTION, DateString+".getDate()",             ResultArray[DATE],       DateCase.getDate() );
	    array[item++] = new TestCase( SECTION, DateString+".getDay()",              ResultArray[DAY],        DateCase.getDay() );
	    array[item++] = new TestCase( SECTION, DateString+".getHours()",            ResultArray[HOURS],      DateCase.getHours() );
	    array[item++] = new TestCase( SECTION, DateString+".getMinutes()",          ResultArray[MINUTES],    DateCase.getMinutes() );
	    array[item++] = new TestCase( SECTION, DateString+".getSeconds()",          ResultArray[SECONDS],    DateCase.getSeconds() );
	    array[item++] = new TestCase( SECTION, DateString+".getMilliseconds()",     ResultArray[MS],         DateCase.getMilliseconds() );

	}

    return ( array );
}
