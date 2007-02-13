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

    var SECTION = "15.9.5.3-2";
    var VERSION = "ECMA_4";
    startTest();
    var TITLE   = "Date.prototype.valueOf";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();
    
function getTestCases() {
    var array = new Array();
    var item = 0;    


    var TZ_ADJUST = TZ_DIFF * msPerHour;
    var now = (new Date()).valueOf();
    var UTC_29_FEB_2000 = TIME_2000 + 31*msPerDay + 28*msPerDay;
    var UTC_1_JAN_2005 = TIME_2000 + TimeInYear(2000) + TimeInYear(2001)+
    TimeInYear(2002)+TimeInYear(2003)+TimeInYear(2004);

    addTestCase2( now );
    addTestCase( TIME_1970 );
    addTestCase( TIME_1900 );
    addTestCase( TIME_2000 );
    addTestCase( UTC_29_FEB_2000 );
    addTestCase( UTC_1_JAN_2005 );

    
    function addTestCase( t ) {
        array[item++] = new TestCase( SECTION,
                                        "(new Date(+t+).valueOf()",
                                        true, t ==
                                        (new Date(t)).valueOf() );
    
        array[item++] = new TestCase( SECTION,
                                        "(new Date(+(t+1)+).valueOf()",
                                        true, t+1 ==
                                        (new Date(t+1)).valueOf() );
    
        array[item++] = new TestCase( SECTION,
                                        "(new Date(+(t-1)+).valueOf()",
                                        true,t-1 ==
                                        (new Date(t-1)).valueOf() );
    
        array[item++] = new TestCase( SECTION,
                                        "(new Date(+(t-TZ_ADJUST)+).valueOf()",
                                        true, t-TZ_ADJUST ==
                                        (new Date(t-TZ_ADJUST)).valueOf() );
    
        array[item++] = new TestCase( SECTION,
                                        "(new Date(+(t+TZ_ADJUST)+).valueOf()",
                                        true, t+TZ_ADJUST ==
                                        (new Date(t+TZ_ADJUST)).valueOf() );
    }
    
    
    // only used for current date and time
    function addTestCase2( t ) {
        array[item++] = new TestCase( SECTION,
                                        "(new Date(now).valueOf()",
                                        true, t ==
                                        (new Date(t)).valueOf() );
    
        array[item++] = new TestCase( SECTION,
                                        "(new Date(now+1).valueOf()",
                                        true, t+1 ==
                                        (new Date(t+1)).valueOf() );
    
        array[item++] = new TestCase( SECTION,
                                        "(new Date(now-1).valueOf()",
                                        true, t-1 ==
                                        (new Date(t-1)).valueOf() );
    
        array[item++] = new TestCase( SECTION,
                                        "(new Date(now-TZ_ADJUST).valueOf()",
                                        true, t-TZ_ADJUST==
                                        (new Date(t-TZ_ADJUST)).valueOf() );
    
        array[item++] = new TestCase( SECTION,
                                        "(new Date(now+TZ_ADJUST).valueOf()",
                                        true, t+TZ_ADJUST ==
                                        (new Date(t+TZ_ADJUST)).valueOf() );
    }
    return array;
}
function MyObject( value ) {
    this.value = value;
    this.valueOf = Date.prototype.valueOf;

//  Changed new Function() to function() { }    
    this.toString = function() { "return this+\"\";" };
    return this;
}

