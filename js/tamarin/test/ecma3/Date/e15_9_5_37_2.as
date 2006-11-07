/* ***** BEGIN LICENSE BLOCK ***** 
 Version: MPL 1.1/GPL 2.0/LGPL 2.1 

The contents of this file are subject to the Mozilla Public License Version 1.1 (the 
"License"); you may not use this file except in compliance with the License. You may obtain 
a copy of the License at http://www.mozilla.org/MPL/ 

Software distributed under the License is distributed on an "AS IS" basis, WITHOUT 
WARRANTY OF ANY KIND, either express or implied. See the License for the specific 
language governing rights and limitations under the License. 

The Original Code is [Open Source Virtual Machine.] 

The Initial Developer of the Original Code is Adobe System Incorporated.  Portions created 
by the Initial Developer are Copyright (C)[ 2005-2006 ] Adobe Systems Incorporated. All Rights 
Reserved. 

Contributor(s): Adobe AS3 Team

Alternatively, the contents of this file may be used under the terms of either the GNU 
General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public 
License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the 
LGPL are applicable instead of those above. If you wish to allow use of your version of this 
file only under the terms of either the GPL or the LGPL, and not to allow others to use your 
version of this file under the terms of the MPL, indicate your decision by deleting provisions 
above and replace them with the notice and other provisions required by the GPL or the 
LGPL. If you do not delete the provisions above, a recipient may use your version of this file 
under the terms of any one of the MPL, the GPL or the LGPL. 

 ***** END LICENSE BLOCK ***** */
    var SECTION = "15.9.5.37-1";
    var VERSION = "ECMA_1";
    startTest();

    writeHeaderToLog( SECTION + " Date.prototype.setUTCFullYear(year [, mon [, date ]] )");
    var testcases:Array= getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;
    
    // Dates around 2000

//    addNewTestCase( "TDATE = new Date(0); TDATE.setUTCFullYear(2000);TDATE",
      var TDATE:Date = new Date(0);
      TDATE.setUTCFullYear(2000);
      addNewTestCase(TDATE,
		    UTCDateFromTime(SetUTCFullYear(0,2000)),
                    LocalDateFromTime(SetUTCFullYear(0,2000)) );

//    addNewTestCase( "TDATE = new Date(0); TDATE.setUTCFullYear(2001);TDATE",
      TDATE = new Date(0);
      TDATE.setUTCFullYear(2001);
      addNewTestCase(TDATE, 
		    UTCDateFromTime(SetUTCFullYear(0,2001)),
                    LocalDateFromTime(SetUTCFullYear(0,2001)) );

//    addNewTestCase( "TDATE = new Date(0); TDATE.setUTCFullYear(1999);TDATE",
      TDATE = new Date(0);
      TDATE.setUTCFullYear(1999);
      addNewTestCase(TDATE,
		    UTCDateFromTime(SetUTCFullYear(0,1999)),
                    LocalDateFromTime(SetUTCFullYear(0,1999)) );
/*
    // Dates around 29 February 2000

    var UTC_FEB_29_1972 = TIME_1970 + TimeInYear(1970) + TimeInYear(1971) +
    31*msPerDay + 28*msPerDay;

    var PST_FEB_29_1972 = UTC_FEB_29_1972 - TZ_DIFF * msPerHour;

    addNewTestCase( "TDATE = new Date("+UTC_FEB_29_1972+"); "+
                    "TDATE.setUTCFullYear(2000);TDATE",
                    UTCDateFromTime(SetUTCFullYear(UTC_FEB_29_1972,2000)),
                    LocalDateFromTime(SetUTCFullYear(UTC_FEB_29_1972,2000)) );

    addNewTestCase( "TDATE = new Date("+PST_FEB_29_1972+"); "+
                    "TDATE.setUTCFullYear(2000);TDATE",
                    UTCDateFromTime(SetUTCFullYear(PST_FEB_29_1972,2000)),
                    LocalDateFromTime(SetUTCFullYear(PST_FEB_29_1972,2000)) );

    // Dates around 2005

    addNewTestCase( "TDATE = new Date(0); TDATE.setUTCFullYear(2005);TDATE",
                    UTCDateFromTime(SetUTCFullYear(0,2005)),
                    LocalDateFromTime(SetUTCFullYear(0,2005)) );

    addNewTestCase( "TDATE = new Date(0); TDATE.setUTCFullYear(2004);TDATE",
                    UTCDateFromTime(SetUTCFullYear(0,2004)),
                    LocalDateFromTime(SetUTCFullYear(0,2004)) );

    addNewTestCase( "TDATE = new Date(0); TDATE.setUTCFullYear(2006);TDATE",
                    UTCDateFromTime(SetUTCFullYear(0,2006)),
                    LocalDateFromTime(SetUTCFullYear(0,2006)) );


    // Dates around 1900
    addNewTestCase( "TDATE = new Date(0); TDATE.setUTCFullYear(1900);TDATE",
                    UTCDateFromTime(SetUTCFullYear(0,1900)),
                    LocalDateFromTime(SetUTCFullYear(0,1900)) );

    addNewTestCase( "TDATE = new Date(0); TDATE.setUTCFullYear(1899);TDATE",
                    UTCDateFromTime(SetUTCFullYear(0,1899)),
                    LocalDateFromTime(SetUTCFullYear(0,1899)) );

    addNewTestCase( "TDATE = new Date(0); TDATE.setUTCFullYear(1901);TDATE",
                    UTCDateFromTime(SetUTCFullYear(0,1901)),
                    LocalDateFromTime(SetUTCFullYear(0,1901)) );

*/

    function addNewTestCase( DateString, UTCDate, LocalDate) {
    //    DateCase = ( DateString );
          var DateCase = DateString ;
    
    //    fixed_year = ( ExpectDate.year >=1900 || ExpectDate.year < 2000 ) ? ExpectDate.year - 1900 : ExpectDate.year;
    
        array[item++] = new TestCase( SECTION, "DateString+.getTime()",             UTCDate.value,       DateCase.getTime() );
        array[item++] = new TestCase( SECTION, "DateString+.valueOf()",             UTCDate.value,       DateCase.valueOf() );
    
        array[item++] = new TestCase( SECTION, "DateString+.getUTCFullYear()",      UTCDate.year,    DateCase.getUTCFullYear() );
        array[item++] = new TestCase( SECTION, "DateString+.getUTCMonth()",         UTCDate.month,  DateCase.getUTCMonth() );
        array[item++] = new TestCase( SECTION, "DateString+.getUTCDate()",          UTCDate.date,   DateCase.getUTCDate() );
        array[item++] = new TestCase( SECTION, "DateString+.getUTCDay()",           UTCDate.day,    DateCase.getUTCDay() );
        array[item++] = new TestCase( SECTION, "DateString+.getUTCHours()",         UTCDate.hours,  DateCase.getUTCHours() );
        array[item++] = new TestCase( SECTION, "DateString+.getUTCMinutes()",       UTCDate.minutes,DateCase.getUTCMinutes() );
        array[item++] = new TestCase( SECTION, "DateString+.getUTCSeconds()",       UTCDate.seconds,DateCase.getUTCSeconds() );
        array[item++] = new TestCase( SECTION, "DateString+.getUTCMilliseconds()",  UTCDate.ms,     DateCase.getUTCMilliseconds() );
    
        array[item++] = new TestCase( SECTION, "DateString+.getFullYear()",         LocalDate.year,       DateCase.getFullYear() );
        array[item++] = new TestCase( SECTION, "DateString+.getMonth()",            LocalDate.month,      DateCase.getMonth() );
        array[item++] = new TestCase( SECTION, "DateString+.getDate()",             LocalDate.date,       DateCase.getDate() );
        array[item++] = new TestCase( SECTION, "DateString+.getDay()",              LocalDate.day,        DateCase.getDay() );
        array[item++] = new TestCase( SECTION, "DateString+.getHours()",            LocalDate.hours,      DateCase.getHours() );
        array[item++] = new TestCase( SECTION, "DateString+.getMinutes()",          LocalDate.minutes,    DateCase.getMinutes() );
        array[item++] = new TestCase( SECTION, "DateString+.getSeconds()",          LocalDate.seconds,    DateCase.getSeconds() );
        array[item++] = new TestCase( SECTION, "DateString+.getMilliseconds()",     LocalDate.ms,         DateCase.getMilliseconds() );
    
        array[item++] = new TestCase( SECTION,
                                          "DateString+.myToString=Object.prototype.toString;+DateString+.toString()",
                                          "[object Date]",(DateCase.myToString=Object.prototype.toString,
                                          DateCase.myToString()));
    
        DateCase.toString = Object.prototype.toString;
    	array[item++]  =new TestCase(SECTION,"DateCase.toString = Object.prototype.toString; DateCase.toString()","[object Date]", DateCase.toString() );
       
        
    }
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
function SetUTCFullYear( t, year, mon, date ) {
    var T = ( t != t ) ? 0 : t;
    var YEAR = Number(year);
    var MONTH = ( mon == void 0 ) ?     MonthFromTime(T) : Number( mon );
    var DATE  = ( date == void 0 ) ?    DateFromTime(T)  : Number( date );
    var DAY = MakeDay( YEAR, MONTH, DATE );

    return ( TimeClip(MakeDate(DAY, TimeWithinDay(T))) );
}
