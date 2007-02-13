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
    var SECTION = "15.9.5.34-1";
    var VERSION = "ECMA_1";
    startTest();

    writeHeaderToLog( SECTION + " Date.prototype.setMonth(mon [, date ] )");

    var now =  (new Date()).valueOf();
    var testcases:Array = getTestCases();



    test();





function getTestCases() {
    var array = new Array();
    var item = 0;
    
    getFunctionCases();
    
    // regression test for http://scopus.mcom.com/bugsplat/show_bug.cgi?id=112404
    var d:Date = new Date(0);


    d.setMonth(1,1);


    addNewTestCase(
        "TDATE = new Date(0); TDATE.setMonth(1,1); TDATE",
        UTCDateFromTime(SetMonth(0,1,1)),
        LocalDateFromTime(SetMonth(0,1,1)) );

     var d:Date = new Date(0);


    d.setMonth(11);

     addNewTestCase1(
        "TDATE = new Date(0); TDATE.setMonth(11); TDATE",
        UTCDateFromTime(SetMonth(0,11)),
        LocalDateFromTime(SetMonth(0,11)) );

    // whatever today is

    addNewTestCase2( "TDATE = new Date(now); (TDATE).setMonth(11,31); TDATE",
                    UTCDateFromTime(SetMonth(now,11,31)),
                    LocalDateFromTime(SetMonth(now,11,31)) );

    addNewTestCase5( "TDATE = new Date(now); (TDATE).setMonth(1); TDATE",
                    UTCDateFromTime(SetMonth(now,1)),
                    LocalDateFromTime(SetMonth(now,1)) );

    // 1970

    addNewTestCase3( "TDATE = new Date(0);(TDATE).setMonth(0,1);TDATE",
                    UTCDateFromTime(SetMonth(0,0,1)),
                    LocalDateFromTime(SetMonth(0,0,1)) );

    addNewTestCase4( "TDATE = new Date("+TIME_1900+"); "+
                    "(TDATE).setMonth(11,31); TDATE",
                    UTCDateFromTime( SetMonth(TIME_1900,11,31) ),
                    LocalDateFromTime( SetMonth(TIME_1900,11,31) ) );

    addNewTestCase6( "TDATE = new Date("+TIME_1900+"); "+
                    "(TDATE).setMonth(1); TDATE",
                    UTCDateFromTime( SetMonth(TIME_1900,1) ),
                    LocalDateFromTime( SetMonth(TIME_1900,1) ) );
    //2000

    addNewTestCase7( "TDATE = new Date("+TIME_2000+");"+
                    "(TDATE).setMonth(1); TDATE",
                    UTCDateFromTime( SetMonth(TIME_2000,1) ),
                    LocalDateFromTime( SetMonth(TIME_2000,1) ) );

    addNewTestCase8( "TDATE = new Date("+TIME_2000+");"+
                    "(TDATE).setMonth(11,31); TDATE",
                    UTCDateFromTime( SetMonth(TIME_2000,11,31) ),
                    LocalDateFromTime( SetMonth(TIME_2000,11,31) ) );

    
    function getFunctionCases() {
        // some tests for all functions
        array[item++] = new TestCase(
                                        SECTION,
                                        "Date.prototype.setMonth.length",
                                        2,
                                        Date.prototype.setMonth.length );
    
        array[item++] = new TestCase(
                                        SECTION,
                                        "typeof Date.prototype.setMonth",
                                        "function",
                                        typeof Date.prototype.setMonth );
    
    }

    function addNewTestCase( DateString, UTCDate, LocalDate) {
        var TDATE:Date = new Date(0);
    	TDATE.setMonth(1,1);
        var DateCase:Date = new Date()
    	DateCase = TDATE;
        var thisError:String = "no error";
    
        array[item++] = new TestCase( SECTION, DateString+".getTime()",             true, UTCDate.value ==       DateCase.getTime() );
        array[item++] = new TestCase( SECTION, DateString+".valueOf()",             true, UTCDate.value ==       DateCase.valueOf() );
    
        array[item++] = new TestCase( SECTION, DateString+".getUTCFullYear()",      true, UTCDate.year ==    DateCase.getUTCFullYear() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCMonth()",         true, UTCDate.month ==  DateCase.getUTCMonth() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCDate()",          true, UTCDate.date ==   DateCase.getUTCDate() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCDay()",           true, UTCDate.day ==    DateCase.getUTCDay() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCHours()",         true, UTCDate.hours ==  DateCase.getUTCHours() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCMinutes()",       true, UTCDate.minutes == DateCase.getUTCMinutes() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCSeconds()",       true, UTCDate.seconds == DateCase.getUTCSeconds() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCMilliseconds()",  true, UTCDate.ms ==     DateCase.getUTCMilliseconds() );
    
        array[item++] = new TestCase( SECTION, DateString+".getFullYear()",         true, LocalDate.year ==       DateCase.getFullYear() );
        array[item++] = new TestCase( SECTION, DateString+".getMonth()",            true, LocalDate.month ==      DateCase.getMonth() );
        array[item++] = new TestCase( SECTION, DateString+".getDate()",             true, LocalDate.date ==       DateCase.getDate() );
        array[item++] = new TestCase( SECTION, DateString+".getDay()",              true, LocalDate.day ==        DateCase.getDay() );
        array[item++] = new TestCase( SECTION, DateString+".getHours()",            true, LocalDate.hours ==      DateCase.getHours() );
        array[item++] = new TestCase( SECTION, DateString+".getMinutes()",          true, LocalDate.minutes ==    DateCase.getMinutes() );
        array[item++] = new TestCase( SECTION, DateString+".getSeconds()",          true, LocalDate.seconds ==    DateCase.getSeconds() );
        array[item++] = new TestCase( SECTION, DateString+".getMilliseconds()",     true, LocalDate.ms ==         DateCase.getMilliseconds() );
    
         array[item++] = new TestCase( SECTION,
                                          DateString+".toString=Object.prototype.toString;"+DateString+".toString()",
                                          "[object Date]",(DateCase.myString=Object.prototype.toString,
                                          DateCase.myString()) );
                         
        DateCase.toString = Object.prototype.toString;
    	array[item++]  =new TestCase(SECTION,DateString+".toString=Object.prototype.toString;"+DateString+".toString()","[object Date]", DateCase.toString() );
                       
    
    
    
    }

    function addNewTestCase1( DateString, UTCDate, LocalDate) {
        var TDATE:Date = new Date(0);
    	(TDATE).setMonth(11);
        var DateCase:Date = new Date()
    	DateCase = TDATE;
        var thisError:String = "no error";
    
        array[item++] = new TestCase( SECTION, DateString+".getTime()",             true, UTCDate.value ==    DateCase.getTime() );
        array[item++] = new TestCase( SECTION, DateString+".valueOf()",             true, UTCDate.value ==      DateCase.valueOf() );
    
        array[item++] = new TestCase( SECTION, DateString+".getUTCFullYear()",      true, UTCDate.year ==   DateCase.getUTCFullYear() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCMonth()",         true, UTCDate.month ==  DateCase.getUTCMonth() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCDate()",          true, UTCDate.date ==  DateCase.getUTCDate() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCDay()",           true, UTCDate.day ==    DateCase.getUTCDay() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCHours()",         true, UTCDate.hours ==  DateCase.getUTCHours() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCMinutes()",       true, UTCDate.minutes == DateCase.getUTCMinutes() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCSeconds()",       true, UTCDate.seconds == DateCase.getUTCSeconds() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCMilliseconds()",  true, UTCDate.ms ==    DateCase.getUTCMilliseconds() );
    
        array[item++] = new TestCase( SECTION, DateString+".getFullYear()",         true, LocalDate.year ==     DateCase.getFullYear() );
        array[item++] = new TestCase( SECTION, DateString+".getMonth()",            true, LocalDate.month ==      DateCase.getMonth() );
        array[item++] = new TestCase( SECTION, DateString+".getDate()",             true, LocalDate.date ==     DateCase.getDate() );
        array[item++] = new TestCase( SECTION, DateString+".getDay()",              true, LocalDate.day ==       DateCase.getDay() );
        array[item++] = new TestCase( SECTION, DateString+".getHours()",            true, LocalDate.hours ==     DateCase.getHours() );
        array[item++] = new TestCase( SECTION, DateString+".getMinutes()",          true, LocalDate.minutes == DateCase.getMinutes() );
        array[item++] = new TestCase( SECTION, DateString+".getSeconds()",          true, LocalDate.seconds ==   DateCase.getSeconds() );
        array[item++] = new TestCase( SECTION, DateString+".getMilliseconds()",     true, LocalDate.ms ==      DateCase.getMilliseconds() );
    
         array[item++] = new TestCase( SECTION,
                                          DateString+".toString=Object.prototype.toString;"+DateString+".toString()",
                                          "[object Date]",(DateCase.myString=Object.prototype.toString,
                                          DateCase.myString()) );
    
        DateCase.toString = Object.prototype.toString;
    	array[item++]  =new TestCase(SECTION,DateString+".toString=Object.prototype.toString;"+DateString+".toString()","[object Date]", DateCase.toString() );
     
    
    }
    
    function addNewTestCase2( DateString, UTCDate, LocalDate) {
        var TDATE:Date = new Date(now);
    	(TDATE).setMonth(11,31);
        var DateCase:Date = new Date();
    	DateCase = TDATE;
        var thisError:String = "no error";
    
        array[item++] = new TestCase( SECTION, DateString+".getTime()",             true, UTCDate.value ==   DateCase.getTime() );
        array[item++] = new TestCase( SECTION, DateString+".valueOf()",             true, UTCDate.value ==     DateCase.valueOf() );
    
        array[item++] = new TestCase( SECTION, DateString+".getUTCFullYear()",      true, UTCDate.year ==   DateCase.getUTCFullYear() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCMonth()",         true, UTCDate.month ==  DateCase.getUTCMonth() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCDate()",          true, UTCDate.date ==   DateCase.getUTCDate() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCDay()",           true, UTCDate.day ==    DateCase.getUTCDay() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCHours()",         true, UTCDate.hours ==  DateCase.getUTCHours() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCMinutes()",       true, UTCDate. minutes ==DateCase.getUTCMinutes() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCSeconds()",       true, UTCDate.seconds ==DateCase.getUTCSeconds() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCMilliseconds()",  true, UTCDate.ms ==     DateCase.getUTCMilliseconds() );
    
        array[item++] = new TestCase( SECTION, DateString+".getFullYear()",         true, LocalDate.year ==       DateCase.getFullYear() );
        array[item++] = new TestCase( SECTION, DateString+".getMonth()",            true, LocalDate.month ==      DateCase.getMonth() );
        array[item++] = new TestCase( SECTION, DateString+".getDate()",             true, LocalDate.date ==       DateCase.getDate() );
        array[item++] = new TestCase( SECTION, DateString+".getDay()",              true, LocalDate.day ==        DateCase.getDay() );
        array[item++] = new TestCase( SECTION, DateString+".getHours()",            true, LocalDate.hours ==      DateCase.getHours() );
        array[item++] = new TestCase( SECTION, DateString+".getMinutes()",          true, LocalDate.minutes ==    DateCase.getMinutes() );
        array[item++] = new TestCase( SECTION, DateString+".getSeconds()",          true, LocalDate.seconds ==    DateCase.getSeconds() );
        array[item++] = new TestCase( SECTION, DateString+".getMilliseconds()",     true, LocalDate.ms ==         DateCase.getMilliseconds() );
    
      array[item++] = new TestCase( SECTION,
                                          DateString+".toString=Object.prototype.toString;"+DateString+".toString()",
                                          "[object Date]",(DateCase.myString = Object.prototype.toString,DateCase.myString()) );
    
        DateCase.toString = Object.prototype.toString;
    	array[item++]  =new TestCase(SECTION,DateString+".toString=Object.prototype.toString;"+DateString+".toString()","[object Date]", DateCase.toString() );
                          
    }
    
    function addNewTestCase5( DateString, UTCDate, LocalDate) {
        var TDATE:Date = new Date(now);
    	(TDATE).setMonth(1);
        var DateCase:Date = new Date();
    	DateCase = TDATE;
        var thisError:String = "no error";
    
        array[item++] = new TestCase( SECTION, DateString+".getTime()",             true, UTCDate.value ==       DateCase.getTime() );
        array[item++] = new TestCase( SECTION, DateString+".valueOf()",             true, UTCDate.value ==       DateCase.valueOf() );
    
        array[item++] = new TestCase( SECTION, DateString+".getUTCFullYear()",      true, UTCDate.year ==    DateCase.getUTCFullYear() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCMonth()",         true, UTCDate.month ==  DateCase.getUTCMonth() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCDate()",          true, UTCDate.date ==   DateCase.getUTCDate() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCDay()",           true, UTCDate.day ==    DateCase.getUTCDay() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCHours()",         true, UTCDate.hours ==  DateCase.getUTCHours() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCMinutes()",       true, UTCDate. minutes ==DateCase.getUTCMinutes() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCSeconds()",       true, UTCDate.seconds ==DateCase.getUTCSeconds() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCMilliseconds()",  true, UTCDate.ms ==     DateCase.getUTCMilliseconds() );
    
        array[item++] = new TestCase( SECTION, DateString+".getFullYear()",         true, LocalDate.year ==       DateCase.getFullYear() );
        array[item++] = new TestCase( SECTION, DateString+".getMonth()",            true, LocalDate.month ==      DateCase.getMonth() );
        array[item++] = new TestCase( SECTION, DateString+".getDate()",             true, LocalDate.date ==       DateCase.getDate() );
        array[item++] = new TestCase( SECTION, DateString+".getDay()",              true, LocalDate.day ==        DateCase.getDay() );
        array[item++] = new TestCase( SECTION, DateString+".getHours()",            true, LocalDate.hours ==      DateCase.getHours() );
        array[item++] = new TestCase( SECTION, DateString+".getMinutes()",          true, LocalDate.minutes ==    DateCase.getMinutes() );
        array[item++] = new TestCase( SECTION, DateString+".getSeconds()",          true, LocalDate.seconds ==    DateCase.getSeconds() );
        array[item++] = new TestCase( SECTION, DateString+".getMilliseconds()",     true, LocalDate.ms ==         DateCase.getMilliseconds() );
    
      array[item++] = new TestCase( SECTION,
                                          DateString+".toString=Object.prototype.toString;"+DateString+".toString()",
                                          "[object Date]",(DateCase.myString = Object.prototype.toString,DateCase.myString()) );
    
        DateCase.toString = Object.prototype.toString;
    	array[item++]  =new TestCase(SECTION,DateString+".toString=Object.prototype.toString;"+DateString+".toString()","[object Date]", DateCase.toString() );
     
    
    }
    
    
    function addNewTestCase3( DateString, UTCDate, LocalDate) {
        //DateCase = eval( DateString );
        var TDATE:Date = new Date(0);
    	(TDATE).setMonth(0,1);
        var Datecase:Date = new Date();
    	DateCase = TDATE;
        var thisError:String = "no error";
    
        array[item++] = new TestCase( SECTION, DateString+".getTime()",             true, UTCDate.value ==       DateCase.getTime() );
        array[item++] = new TestCase( SECTION, DateString+".valueOf()",             true, UTCDate.value ==       DateCase.valueOf() );
    
        array[item++] = new TestCase( SECTION, DateString+".getUTCFullYear()",      true, UTCDate.year ==    DateCase.getUTCFullYear() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCMonth()",         true, UTCDate.month ==  DateCase.getUTCMonth() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCDate()",          true, UTCDate.date ==   DateCase.getUTCDate() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCDay()",           true, UTCDate.day ==    DateCase.getUTCDay() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCHours()",         true, UTCDate.hours ==  DateCase.getUTCHours() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCMinutes()",       true, UTCDate. minutes ==DateCase.getUTCMinutes() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCSeconds()",       true, UTCDate.seconds ==DateCase.getUTCSeconds() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCMilliseconds()",  true, UTCDate.ms ==     DateCase.getUTCMilliseconds() );
    
        array[item++] = new TestCase( SECTION, DateString+".getFullYear()",         true, LocalDate.year ==       DateCase.getFullYear() );
        array[item++] = new TestCase( SECTION, DateString+".getMonth()",            true, LocalDate.month ==      DateCase.getMonth() );
        array[item++] = new TestCase( SECTION, DateString+".getDate()",             true, LocalDate.date ==       DateCase.getDate() );
        array[item++] = new TestCase( SECTION, DateString+".getDay()",              true, LocalDate.day ==        DateCase.getDay() );
        array[item++] = new TestCase( SECTION, DateString+".getHours()",            true, LocalDate.hours ==      DateCase.getHours() );
        array[item++] = new TestCase( SECTION, DateString+".getMinutes()",          true, LocalDate.minutes ==    DateCase.getMinutes() );
        array[item++] = new TestCase( SECTION, DateString+".getSeconds()",          true, LocalDate.seconds ==    DateCase.getSeconds() );
        array[item++] = new TestCase( SECTION, DateString+".getMilliseconds()",     true, LocalDate.ms ==         DateCase.getMilliseconds() );
    
        array[item++] = new TestCase( SECTION,
                                          DateString+".toString=Object.prototype.toString;"+DateString+".toString()",
                                          "[object Date]",(DateCase.myString=Object.prototype.toString,DateCase.myString()) );
    
        DateCase.toString = Object.prototype.toString;
    	array[item++]  =new TestCase(SECTION,DateString+".toString=Object.prototype.toString;"+DateString+".toString()","[object Date]", DateCase.toString() );
     
    
    
    }
    
    
    function addNewTestCase4( DateString, UTCDate, LocalDate) {
        //DateCase = eval( DateString );
        var TDATE:Date = new Date(TIME_1900);
        (TDATE).setMonth(11,31);
        var DateCase:Date = new Date();
    	DateCase = TDATE;
        var thisError:String = "no error";
    
        array[item++] = new TestCase( SECTION, DateString+".getTime()",             true, UTCDate.value ==       DateCase.getTime() );
        array[item++] = new TestCase( SECTION, DateString+".valueOf()",             true, UTCDate.value ==       DateCase.valueOf() );
    
        array[item++] = new TestCase( SECTION, DateString+".getUTCFullYear()",      true, UTCDate.year ==    DateCase.getUTCFullYear() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCMonth()",         true, UTCDate.month ==  DateCase.getUTCMonth() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCDate()",          true, UTCDate.date ==   DateCase.getUTCDate() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCDay()",           true, UTCDate.day ==    DateCase.getUTCDay() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCHours()",         true, UTCDate.hours ==  DateCase.getUTCHours() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCMinutes()",       true, UTCDate. minutes ==DateCase.getUTCMinutes() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCSeconds()",       true, UTCDate.seconds ==DateCase.getUTCSeconds() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCMilliseconds()",  true, UTCDate.ms ==     DateCase.getUTCMilliseconds() );
    
        array[item++] = new TestCase( SECTION, DateString+".getFullYear()",         true, LocalDate.year ==       DateCase.getFullYear() );
        array[item++] = new TestCase( SECTION, DateString+".getMonth()",            true, LocalDate.month ==      DateCase.getMonth() );
        array[item++] = new TestCase( SECTION, DateString+".getDate()",             true, LocalDate.date ==       DateCase.getDate() );
        array[item++] = new TestCase( SECTION, DateString+".getDay()",              true, LocalDate.day ==        DateCase.getDay() );
        array[item++] = new TestCase( SECTION, DateString+".getHours()",            true, LocalDate.hours ==      DateCase.getHours() );
        array[item++] = new TestCase( SECTION, DateString+".getMinutes()",          true, LocalDate.minutes ==    DateCase.getMinutes() );
        array[item++] = new TestCase( SECTION, DateString+".getSeconds()",          true, LocalDate.seconds ==    DateCase.getSeconds() );
        array[item++] = new TestCase( SECTION, DateString+".getMilliseconds()",     true, LocalDate.ms ==         DateCase.getMilliseconds() );
    
        array[item++] = new TestCase( SECTION,
                                          DateString+".toString=Object.prototype.toString;"+DateString+".toString()",
                                          "[object Date]",(DateCase.myString=Object.prototype.toString,
                                          DateCase.myString() ));
    
        DateCase.toString = Object.prototype.toString;
    	array[item++]  =new TestCase(SECTION,DateString+".toString=Object.prototype.toString;"+DateString+".toString()","[object Date]", DateCase.toString() );
     
    
    }
    function addNewTestCase6( DateString, UTCDate, LocalDate) {
    
        var TDATE:Date = new Date(TIME_1900);
        (TDATE).setMonth(1);
        var DateCase:Date = new Date();
    	DateCase = TDATE;
        var thisError:String = "no error";
    
        array[item++] = new TestCase( SECTION, DateString+".getTime()",             true, UTCDate.value ==       DateCase.getTime() );
        array[item++] = new TestCase( SECTION, DateString+".valueOf()",             true, UTCDate.value ==       DateCase.valueOf() );
    
        array[item++] = new TestCase( SECTION, DateString+".getUTCFullYear()",      true, UTCDate.year ==    DateCase.getUTCFullYear() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCMonth()",         true, UTCDate.month ==  DateCase.getUTCMonth() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCDate()",          true, UTCDate.date ==   DateCase.getUTCDate() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCDay()",           true, UTCDate.day ==    DateCase.getUTCDay() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCHours()",         true, UTCDate.hours ==  DateCase.getUTCHours() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCMinutes()",       true, UTCDate. minutes ==DateCase.getUTCMinutes() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCSeconds()",       true, UTCDate.seconds ==DateCase.getUTCSeconds() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCMilliseconds()",  true, UTCDate.ms ==     DateCase.getUTCMilliseconds() );
    
        array[item++] = new TestCase( SECTION, DateString+".getFullYear()",         true, LocalDate.year ==       DateCase.getFullYear() );
        array[item++] = new TestCase( SECTION, DateString+".getMonth()",            true, LocalDate.month ==      DateCase.getMonth() );
        array[item++] = new TestCase( SECTION, DateString+".getDate()",             true, LocalDate.date ==       DateCase.getDate() );
        array[item++] = new TestCase( SECTION, DateString+".getDay()",              true, LocalDate.day ==        DateCase.getDay() );
        array[item++] = new TestCase( SECTION, DateString+".getHours()",            true, LocalDate.hours ==      DateCase.getHours() );
        array[item++] = new TestCase( SECTION, DateString+".getMinutes()",          true, LocalDate.minutes ==    DateCase.getMinutes() );
        array[item++] = new TestCase( SECTION, DateString+".getSeconds()",          true, LocalDate.seconds ==    DateCase.getSeconds() );
        array[item++] = new TestCase( SECTION, DateString+".getMilliseconds()",     true, LocalDate.ms ==         DateCase.getMilliseconds() );
    
        array[item++] = new TestCase( SECTION,
                                          DateString+".toString=Object.prototype.toString;"+DateString+".toString()",
                                          "[object Date]",(DateCase.myString=Object.prototype.toString,
                                          DateCase.myString() ));
    
        DateCase.toString = Object.prototype.toString;
    	array[item++]  =new TestCase(SECTION,DateString+".toString=Object.prototype.toString;"+DateString+".toString()","[object Date]", DateCase.toString() );
                          
    }
    
    function addNewTestCase8( DateString, UTCDate, LocalDate) {
    
        var TDATE:Date = new Date(TIME_2000);
        (TDATE).setMonth(11,31);
        var DateCase:Date = new Date();
    	DateCase = TDATE;
        var thisError:String = "no error";
    
        array[item++] = new TestCase( SECTION, DateString+".getTime()",             true, UTCDate.value ==       DateCase.getTime() );
        array[item++] = new TestCase( SECTION, DateString+".valueOf()",             true, UTCDate.value ==       DateCase.valueOf() );
    
        array[item++] = new TestCase( SECTION, DateString+".getUTCFullYear()",      true, UTCDate.year ==    DateCase.getUTCFullYear() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCMonth()",         true, UTCDate.month ==  DateCase.getUTCMonth() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCDate()",          true, UTCDate.date ==   DateCase.getUTCDate() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCDay()",           true, UTCDate.day ==    DateCase.getUTCDay() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCHours()",         true, UTCDate.hours ==  DateCase.getUTCHours() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCMinutes()",       true, UTCDate. minutes ==DateCase.getUTCMinutes() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCSeconds()",       true, UTCDate.seconds ==DateCase.getUTCSeconds() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCMilliseconds()",  true, UTCDate.ms ==     DateCase.getUTCMilliseconds() );
    
        array[item++] = new TestCase( SECTION, DateString+".getFullYear()",         true, LocalDate.year ==       DateCase.getFullYear() );
        array[item++] = new TestCase( SECTION, DateString+".getMonth()",            true, LocalDate.month ==      DateCase.getMonth() );
        array[item++] = new TestCase( SECTION, DateString+".getDate()",             true, LocalDate.date ==       DateCase.getDate() );
        array[item++] = new TestCase( SECTION, DateString+".getDay()",              true, LocalDate.day ==        DateCase.getDay() );
        array[item++] = new TestCase( SECTION, DateString+".getHours()",            true, LocalDate.hours ==      DateCase.getHours() );
        array[item++] = new TestCase( SECTION, DateString+".getMinutes()",          true, LocalDate.minutes ==    DateCase.getMinutes() );
        array[item++] = new TestCase( SECTION, DateString+".getSeconds()",          true, LocalDate.seconds ==    DateCase.getSeconds() );
        array[item++] = new TestCase( SECTION, DateString+".getMilliseconds()",     true, LocalDate.ms ==         DateCase.getMilliseconds() );
    
        array[item++] = new TestCase( SECTION,
                                          DateString+".toString=Object.prototype.toString;"+DateString+".toString()",
                                          "[object Date]",(DateCase.myString=Object.prototype.toString,
                                          DateCase.myString() ));
    
        DateCase.toString = Object.prototype.toString;
    	array[item++]  =new TestCase(SECTION,DateString+".toString=Object.prototype.toString;"+DateString+".toString()","[object Date]", DateCase.toString() );
                          
    }
    function addNewTestCase7( DateString, UTCDate, LocalDate) {
    
        var TDATE:Date = new Date(TIME_2000);
        (TDATE).setMonth(1);
        var DateCase:Date = new Date();
    	DateCase = TDATE;
        var thisError:String = "no error";
    
        array[item++] = new TestCase( SECTION, DateString+".getTime()",             true, UTCDate.value ==       DateCase.getTime() );
        array[item++] = new TestCase( SECTION, DateString+".valueOf()",             true, UTCDate.value ==       DateCase.valueOf() );
    
        array[item++] = new TestCase( SECTION, DateString+".getUTCFullYear()",      true, UTCDate.year ==    DateCase.getUTCFullYear() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCMonth()",         true, UTCDate.month ==  DateCase.getUTCMonth() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCDate()",          true, UTCDate.date ==   DateCase.getUTCDate() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCDay()",           true, UTCDate.day ==    DateCase.getUTCDay() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCHours()",         true, UTCDate.hours ==  DateCase.getUTCHours() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCMinutes()",       true, UTCDate. minutes ==DateCase.getUTCMinutes() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCSeconds()",       true, UTCDate.seconds ==DateCase.getUTCSeconds() );
        array[item++] = new TestCase( SECTION, DateString+".getUTCMilliseconds()",  true, UTCDate.ms ==     DateCase.getUTCMilliseconds() );
    
        array[item++] = new TestCase( SECTION, DateString+".getFullYear()",         true, LocalDate.year ==       DateCase.getFullYear() );
        array[item++] = new TestCase( SECTION, DateString+".getMonth()",            true, LocalDate.month ==      DateCase.getMonth() );
        array[item++] = new TestCase( SECTION, DateString+".getDate()",             true, LocalDate.date ==       DateCase.getDate() );
        array[item++] = new TestCase( SECTION, DateString+".getDay()",              true, LocalDate.day ==        DateCase.getDay() );
        array[item++] = new TestCase( SECTION, DateString+".getHours()",            true, LocalDate.hours ==      DateCase.getHours() );
        array[item++] = new TestCase( SECTION, DateString+".getMinutes()",          true, LocalDate.minutes ==    DateCase.getMinutes() );
        array[item++] = new TestCase( SECTION, DateString+".getSeconds()",          true, LocalDate.seconds ==    DateCase.getSeconds() );
        array[item++] = new TestCase( SECTION, DateString+".getMilliseconds()",     true, LocalDate.ms ==         DateCase.getMilliseconds() );
    
        array[item++] = new TestCase( SECTION,
                                          DateString+".toString=Object.prototype.toString;"+DateString+".toString()",
                                          "[object Date]",(DateCase.myString=Object.prototype.toString,
                                          DateCase.myString() ));
    
        DateCase.toString = Object.prototype.toString;
    	array[item++]  =new TestCase(SECTION,DateString+".toString=Object.prototype.toString;"+DateString+".toString()","[object Date]", DateCase.toString() );
                          
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


function SetMonth( t, mon, date ) {
    var TIME = LocalTime(t);
    var MONTH = Number( mon );
    var DATE = ( date == void 0 ) ? DateFromTime(TIME) : Number( date );
    var DAY = MakeDay( YearFromTime(TIME), MONTH, DATE );
    return ( TimeClip (UTC(MakeDate( DAY, TimeWithinDay(TIME) ))) );
}
