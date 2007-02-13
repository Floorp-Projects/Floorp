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
    var SECTION = "15.9.5.23-1";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE = "Date.prototype.setTime()";

    writeHeaderToLog( SECTION + " Date.prototype.setTime(time)");

    var testcases = getTestCases();
    test();

function getTestCases() {
    var now = "now";
    return addTestCase( now, String( TIME_2000 ) );
}

function addTestCase( startTime, setTime ) {
    var array = new Array();
    var item = 0;
    
    if ( startTime == "now" ) {
        DateCase = new Date();
    } else {
        DateCase = new Date( startTime );
    }

    DateCase.setTime( setTime );
    var DateString = "var d = new Date("+startTime+"); d.setTime("+setTime+"); d" ;
    var UTCDate   = UTCDateFromTime ( Number(setTime) );
    var LocalDate = LocalDateFromTime( Number(setTime) );

//    fixed_year = ( ExpectDate.year >=1900 || ExpectDate.year < 2000 ) ? ExpectDate.year - 1900 : ExpectDate.year;

    array[item++] = new TestCase( SECTION, DateString+".getTime()",             UTCDate.value,      DateCase.getTime() );
    array[item++] = new TestCase( SECTION, DateString+".valueOf()",             UTCDate.value,      DateCase.valueOf() );

    array[item++] = new TestCase( SECTION, DateString+".getUTCFullYear()",      UTCDate.year,       DateCase.getUTCFullYear() );
    array[item++] = new TestCase( SECTION, DateString+".getUTCMonth()",         UTCDate.month,      DateCase.getUTCMonth() );
    array[item++] = new TestCase( SECTION, DateString+".getUTCDate()",          UTCDate.date,       DateCase.getUTCDate() );
    array[item++] = new TestCase( SECTION, DateString+".getUTCDay()",           UTCDate.day,        DateCase.getUTCDay() );
    array[item++] = new TestCase( SECTION, DateString+".getUTCHours()",         UTCDate.hours,      DateCase.getUTCHours() );
    array[item++] = new TestCase( SECTION, DateString+".getUTCMinutes()",       UTCDate.minutes,    DateCase.getUTCMinutes() );
    array[item++] = new TestCase( SECTION, DateString+".getUTCSeconds()",       UTCDate.seconds,    DateCase.getUTCSeconds() );
    array[item++] = new TestCase( SECTION, DateString+".getUTCMilliseconds()",  UTCDate.ms,         DateCase.getUTCMilliseconds() );

    array[item++] = new TestCase( SECTION, DateString+".getFullYear()",         LocalDate.year,     DateCase.getFullYear() );
    array[item++] = new TestCase( SECTION, DateString+".getMonth()",            LocalDate.month,    DateCase.getMonth() );
    array[item++] = new TestCase( SECTION, DateString+".getDate()",             LocalDate.date,     DateCase.getDate() );
    array[item++] = new TestCase( SECTION, DateString+".getDay()",              LocalDate.day,      DateCase.getDay() );
    array[item++] = new TestCase( SECTION, DateString+".getHours()",            LocalDate.hours,    DateCase.getHours() );
    array[item++] = new TestCase( SECTION, DateString+".getMinutes()",          LocalDate.minutes,  DateCase.getMinutes() );
    array[item++] = new TestCase( SECTION, DateString+".getSeconds()",          LocalDate.seconds,  DateCase.getSeconds() );
    array[item++] = new TestCase( SECTION, DateString+".getMilliseconds()",     LocalDate.ms,       DateCase.getMilliseconds() );

    DateCase.toString = Object.prototype.toString;
	array[item++]  =new TestCase(SECTION,"DateCase.toString = Object.prototype.toString; DateCase.toString()","[object Date]", DateCase.toString() );

    return array;
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
    var d       = new MyDate();
    d.year      = YearFromTime(t);
    d.month     = MonthFromTime(t);
    d.date      = DateFromTime(t);
    d.hours     = HourFromTime(t);
    d.minutes   = MinFromTime(t);
    d.seconds   = SecFromTime(t);
    d.ms        = msFromTime(t);
    d.time      = MakeTime( d.hours, d.minutes, d.seconds, d.ms );
    d.value     = TimeClip( MakeDate( MakeDay( d.year, d.month, d.date ), d.time ) );
    d.day       = WeekDay( d.value );
    return (d);
}

