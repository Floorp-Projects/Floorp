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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
/**
    File Name:          15.6.4.3.js
    ECMA Section:       15.6.4.3 Boolean.prototype.valueOf()
    Description:        Returns this boolean value.

                        The valueOf function is not generic; it generates
                        a runtime error if its this value is not a Boolean
                        object.  Therefore it cannot be transferred to other
                        kinds of objects for use as a method.

    Author:             christine@netscape.com
    Date:               june 27, 1997
*/

function getTestCases() {
    var array = new Array();
    var item = 0;

    array[item++] = new TestCase( "15.8.6.4",   "new Boolean(1)",       true,   (new Boolean(1)).valueOf() );

    array[item++] = new TestCase( "15.8.6.4",   "new Boolean(0)",       false,  (new Boolean(0)).valueOf() );
    array[item++] = new TestCase( "15.8.6.4",   "new Boolean(-1)",      true,   (new Boolean(-1)).valueOf() );
    array[item++] = new TestCase( "15.8.6.4",   "new Boolean('1')",     true,   (new Boolean("1")).valueOf() );
    array[item++] = new TestCase( "15.8.6.4",   "new Boolean('0')",     true,   (new Boolean("0")).valueOf() );
    array[item++] = new TestCase( "15.8.6.4",   "new Boolean(true)",    true,   (new Boolean(true)).valueOf() );
    array[item++] = new TestCase( "15.8.6.4",   "new Boolean(false)",   false,  (new Boolean(false)).valueOf() );
    array[item++] = new TestCase( "15.8.6.4",   "new Boolean('true')",  true,   (new Boolean("true")).valueOf() );
    array[item++] = new TestCase( "15.8.6.4",   "new Boolean('false')", true,   (new Boolean('false')).valueOf() );

    array[item++] = new TestCase( "15.8.6.4",   "new Boolean('')",      false,  (new Boolean('')).valueOf() );
    array[item++] = new TestCase( "15.8.6.4",   "new Boolean(null)",    false,  (new Boolean(null)).valueOf() );
    array[item++] = new TestCase( "15.8.6.4",   "new Boolean(void(0))", false,  (new Boolean(void(0))).valueOf() );
    array[item++] = new TestCase( "15.8.6.4",   "new Boolean(-Infinity)", true, (new Boolean(Number.NEGATIVE_INFINITY)).valueOf() );
    array[item++] = new TestCase( "15.8.6.4",   "new Boolean(NaN)",     false,  (new Boolean(Number.NaN)).valueOf() );
    array[item++] = new TestCase( "15.8.6.4",   "new Boolean()",        false,  (new Boolean()).valueOf() );

    array[item++] = new TestCase( "15.8.6.4",   "new Boolean(x=1)",     true,   (new Boolean(x=1)).valueOf() );
    array[item++] = new TestCase( "15.8.6.4",   "new Boolean(x=0)",     false,  (new Boolean(x=0)).valueOf() );
    array[item++] = new TestCase( "15.8.6.4",   "new Boolean(x=false)", false,  (new Boolean(x=false)).valueOf() );
    array[item++] = new TestCase( "15.8.6.4",   "new Boolean(x=true)",  true,   (new Boolean(x=true)).valueOf() );
    array[item++] = new TestCase( "15.8.6.4",   "new Boolean(x=null)",  false,  (new Boolean(x=null)).valueOf() );
    array[item++] = new TestCase( "15.8.6.4",   "new Boolean(x='')",    false,  (new Boolean(x="")).valueOf() );
    array[item++] = new TestCase( "15.8.6.4",   "new Boolean(x=' ')",   true,   (new Boolean(x=" ")).valueOf() );

    return ( array );
}

function test( array ) {
        var passed = true;

        writeHeaderToLog("15.8.6.4.3 Properties of the Boolean Object:  valueOf");

        for ( i = 0; i < array.length; i++ ) {

            array[i].passed = writeTestCaseResult(
                    array[i].expect,
                    array[i].actual,
                    "( "+ array[i].description +" ).valueOf() = "+ array[i].actual );

            array[i].reason += ( array[i].passed ) ? "" : "wrong value ";

            passed = ( array[i].passed ) ? passed  : false;

        }

        stopTest();

    //  all tests must return a boolean value
        return ( array );
}

//  for TCMS, the testcases array must be global.
    var testcases = getTestCases();

//  all tests must call a function that returns a boolean value
    test( testcases );
