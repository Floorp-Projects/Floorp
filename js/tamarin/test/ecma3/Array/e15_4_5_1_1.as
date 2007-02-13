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

    var SECTION = "15.4.5.1-1";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "Array [[Put]] (P, V)";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();

    var item = 0;

    // P is "length"

    array[item++] = new TestCase(   SECTION,
                                    "var A = new Array(); A.length = 1000; A.length",
                                    1000,
                                    (A = new Array(), A.length = 1000, A.length ) );

    // A has Property P, and P is not length or an array index
    array[item++] = new TestCase(   SECTION,
                                    "var A = new Array(1000); A.name = 'name of this array'; A.name",
                                    'name of this array',
                                    (A = new Array(1000), A.name = 'name of this array', A.name ) );

    array[item++] = new TestCase(   SECTION,
                                    "var A = new Array(1000); A.name = 'name of this array'; A.length",
                                    1000,
                                    (A = new Array(1000), A.name = 'name of this array', A.length ) );


    // A has Property P, P is not length, P is an array index, and ToUint32(p) is less than the
    // value of length

    array[item++] = new TestCase(   SECTION,
                                    "var A = new Array(1000); A[123] = 'hola'; A[123]",
                                    'hola',
                                    (A = new Array(1000), A[123] = 'hola', A[123] ) );

    array[item++] = new TestCase(   SECTION,
                                    "var A = new Array(1000); A[123] = 'hola'; A.length",
                                    1000,
                                    (A = new Array(1000), A[123] = 'hola', A.length ) );


	/*
    for ( var i = 0X0020, TEST_STRING = "var A = new Array( " ; i < 0x00ff; i++ ) {
        TEST_STRING += "\'\\"+ String.fromCharCode( i ) +"\'";
        if ( i < 0x00FF - 1   ) {
            TEST_STRING += ",";
        } else {
            TEST_STRING += ");"
        }
    }
	*/
	var TEST_STRING;
    var LENGTH = 0x00ff - 0x0020;
    var A = new Array();
    var index = 0;
    for ( var i = 0X0020; i < 0x00ff; i++ ) {
		A[index++] = String.fromCharCode( i );
	}

    array[item++] = new TestCase(   SECTION,
                                    TEST_STRING +" A[150] = 'hello'; A[150]",
                                    'hello',
                                    (A[150] = 'hello', A[150]) );

    array[item++] = new TestCase(   SECTION,
                                    TEST_STRING +" A[150] = 'hello', A.length",
                                    LENGTH,
                                    (A[150] = 'hello', A.length) );

    // A has Property P, P is not length, P is an array index, and ToUint32(p) is not less than the
    // value of length

    array[item++] = new TestCase(   SECTION,
                                    "var A = new Array(); A[123] = true; A.length",
                                    124,
                                    (A = new Array(), A[123] = true, A.length ) );

    array[item++] = new TestCase(   SECTION,
                                    "var A = new Array(0,1,2,3,4,5,6,7,8,9,10); A[15] ='15'; A.length",
                                    16,
                                    (A = new Array(0,1,2,3,4,5,6,7,8,9,10), A[15] ='15', A.length ) );

	// !!@ As of 04jan05, the AVM+ does not like the (i <= 10) line (where '15' is a different type than void 0)
    for ( var i = 0; i < A.length; i++, item++ ) {
		var temp;
		if (i <= 10)
			temp = i;
		else if (i == 15)
			temp = '15';
		else
			temp = void 0;

        array[item] = new TestCase( SECTION,
                                    "var A = new Array(0,1,2,3,4,5,6,7,8,9,10); A[15] ='15'; A[" +i +"]",
                                    temp, //(i <= 10) ? i : ( i == 15 ? '15' : void 0 ),
                                    A[i] );
    }
    // P is not an array index, and P is not "length"

    try{
    	thisErr = "no error";
    	var A = new Array(); A.join.length = 4; A.join.length;
	}catch(e:ReferenceError){
		thisErr = e.toString();
	} finally{
    	array[item++] = new TestCase(SECTION,
                                    "var A = new Array(); A.join.length = 4; A.join.length",
                                    "ReferenceError: Error #1074",
                                    referenceError(thisErr));
    }

    try{
    	thisErr = "no error";
    	var A = new Array(); A.join.length = 4; A.length;
	}catch(e:ReferenceError){
		thisErr = e.toString();
	} finally{
    	array[item++] = new TestCase(SECTION,
                                    "var A = new Array(); A.join.length = 4; A.length",
                                    "ReferenceError: Error #1074",
                                    referenceError(thisErr));
    }

    return array;
}
