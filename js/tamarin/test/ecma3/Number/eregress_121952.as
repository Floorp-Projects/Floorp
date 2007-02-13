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
    var SECTION = "15.7.4.5";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "Number.toFixed";

    writeHeaderToLog( SECTION + " "+TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    array[item++] = new TestCase(   SECTION,
                                    "Number.prototype.toFixed.length",
                                    1,
                                    Number.prototype.toFixed.length );

    var profits = 2489.8237

    array[item++] = new TestCase(   SECTION,
                                    "Number.toFixed -- rounding up",
                                    "2489.824",
                                    profits.toFixed(3)+"" );

    array[item++] = new TestCase(   SECTION,
                                    "Number.toFixed",
                                    "2489.82",
                                    profits.toFixed(2)+"" );

    array[item++] = new TestCase(   SECTION,
                                    "Number.toFixed -- padding",
                                    "2489.8237000",
                                    profits.toFixed(7)+"" );

    array[item++] = new TestCase(   SECTION,
                                    "Number.toFixed -- padding",
                                    "2489.8237000",
                                    profits.toFixed(7)+"" );

    array[item++] = new TestCase(   SECTION,
                                    "Number.toFixed(undefined)",
                                    "2490",
                                    profits.toFixed()+"" );

    array[item++] = new TestCase(   SECTION,
                                    "Number.toFixed(0)",
                                    "2490",
                                    profits.toFixed(0)+"" );
    array[item++] = new TestCase(   SECTION,
                                    "Number.toFixed(null)",
                                    "2490",
                                    profits.toFixed(null)+"" );

    
    var thisError:String="no error"

    try{
        profits.toFixed(-1);
    }catch(e:RangeError){
        thisError=e.toString();
    }finally{
        array[item++] = new TestCase(   SECTION,
                                    "Number.toFixed(0)",
                                    "RangeError: Error #1002",
                                    rangeError(thisError));
    }

    try{
        profits.toFixed(21);
    }catch(e:RangeError){
        thisError=e.toString();
    }finally{
        array[item++] = new TestCase(   SECTION,
                                    "Number.toFixed(21)",
                                    "RangeError: Error #1002",
                                    rangeError(thisError));
    }

    

    var MYNUM=1000000000000000128

    array[item++] = new TestCase(   SECTION,
                                    "Number.toFixed(0)",
                                    "1000000000000000100",
                                    MYNUM.toFixed(0)+"" );

    var MYNUM2 = 4;

    array[item++] = new TestCase(   SECTION,
                                    "Number.toFixed(2)",
                                    "4.00",
                                    MYNUM2.toFixed(2)+"" );

    
    return ( array );
}
