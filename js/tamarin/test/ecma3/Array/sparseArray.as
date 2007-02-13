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
/*
 *  File Name:          sparseArray.as
 *  ECMA Section:       15.4 general Array testcases
 *  Description:        Test Case for generic Array Class when
 *			Arrays are used as sparse arrays with non-consecutive integer
 *			properties or non-numeric properties.
 *			These test when a "dense" array becomes sparse and vice-versa.

 *
 *  Author: 		Werner Sharp (wsharp@macromedia.com)
 *  Date:   		01/28/2005
 *
 */

var SECTION = "15.4";
var TITLE   = "Array sparse tests";

var VERSION = "ECMA_3";

startTest();

writeHeaderToLog( SECTION + " " + TITLE);


var testcases = getTestCases();

test();

function getTestCases() {
	var array = new Array();
	var item = 0;
	var foundOne= false;
	var foundTwo = false;
	var foundThree = false;
	var somethingElse = false;
	var result = false;

	// Test ArrayObject::deleteProperty convert a purely dense array into a sparse array
	var x = new Array(1, 2, 3);
	delete x[1];
	for (var i in x)
	{
		if( x[i] == 1 ){
			foundOne = true;
		} else if (x[i] == 2 ){
			foundTwo = true;
		} else if (x[i] == 3 ){
			foundThree = true;
		} else {
			somethingElse = true;
		}
	}

	if( foundOne && !foundTwo && foundThree && !somethingElse ) result = true;

	// section, description, correct result, actual result
	array[item++] = new TestCase( SECTION, "new Array(1, 2, 3), delete a[1], for in array", true, result );

	// Test ArrayObject::deleteProperty convert a purely dense array into a sparse array
	var x = new Array(1, 2, 3);
	delete x[1];
	for (var i in x)
	{
		if( x[i] == 1 ){
			foundOne = true;
		} else if (x[i] == 2 ){
			foundTwo = true;
		} else if (x[i] == 3 ){
			foundThree = true;
		} else {
			somethingElse = true;
		}
	}

	if( foundOne && !foundTwo && foundThree && !somethingElse ) result = true;

	// section, description, correct result, actual result
	array[item++] = new TestCase( SECTION, "new Array(1, 2, 3), delete a[1], for in array", true, result);

	// Test ArrayObject::deleteProperty convert a purely dense array into a sparse array
	var x = new Array(1, 2, 3);
	delete x[2];
	for (var i in x)
	{
		if( x[i] == 1 ){
			foundOne = true;
		} else if (x[i] == 2 ){
			foundTwo = true;
		} else if (x[i] == 3 ){
			foundThree = true;
		} else {
			somethingElse = true;
		}
	}

	if( foundOne && foundTwo && !foundThree && !somethingElse ) result = true;

	// section, description, correct result, actual result
	array[item++] = new TestCase( SECTION, "new Array(1, 2, 3), delete a[2], for in array", true, result);

	var x = new Array(1, 2, 3);
	delete x[0];
	for (var i in x)
	{
		if( x[i] == 1 ){
			foundOne = true;
		} else if (x[i] == 2 ){
			foundTwo = true;
		} else if (x[i] == 3 ){
			foundThree = true;
		} else {
			somethingElse = true;
		}
	}

	if( !foundOne && foundTwo && foundThree && !somethingElse ) result = true;

	// section, description, correct result, actual result
	// Results are [3 2] because of random hash table ordering.
	array[item++] = new TestCase( SECTION, "new Array(1, 2, 3), delete a[0], for in array", true, result);


	// dense array that has a sparse element added
	var x = new Array(1, 2, 3);
	x[4] = '5';
	array[item++] = new TestCase( SECTION, "new Array(1, 2, 3), x[4] = '5'", "1,2,3,,5", x.toString());

	// dense array that has a sparse element added then another which should convert the whole array
	// into a dense array
	var x = new Array(1, 2, 3);
	x[4] = '5';
	x[3] = '4';
	array[item++] = new TestCase( SECTION, "new Array(1, 2, 3), x[4] = '5', x[3] = '4'", "1,2,3,4,5", x.toString());

	// Mixed mode - part dense and sparse for x array, pure sparse for y array
	var x = new Array();
	x[0] = 0;
	x[2] = 2;
	var y = new Array();
	y[3] = 3
	y[5] = 4;
	var z = y.concat (x);
	array[item++] = new TestCase( SECTION, "create two arrays and concat them together", ",,,3,,4,0,,2", z.toString());

	// Mixed mode - part dense and sparse for each array
	var x = new Array(0,1);
	x[3] = 3;
	var y = new Array(0,1);
	y[6] = 6
	var z = y.concat (x);
	array[item++] = new TestCase( SECTION, "create two arrays and concat them together", "0,1,,,,,6,0,1,,3", z.toString());

	// Mixed mode - part dense and sparse for each array
	var x = new Array(0,1);
	x[3] = 3;
	var z = x.concat (10);
	array[item++] = new TestCase( SECTION, "create two arrays and concat them together", "0,1,,3,10", z.toString());

	// shift should convert sparse array into dense array
	var x = new Array();
	x[1] = 1;
	x[2] = 2;
	x[3] = 3;
	x[4] = 4;
	z = x.shift();
	array[item++] = new TestCase( SECTION, "create array with 1-4 populated, then shift, array.toString", "1,2,3,4", x.toString());
	array[item++] = new TestCase( SECTION, "create array with 1-4 populated, then shift, get results", undefined, z);

	// pop of mixed mode array
	var x = new Array();
	x[1] = 1;
	x[2] = 2;
	x[3] = 3;
	z = x.pop();
	array[item++] = new TestCase( SECTION, "create array with 1-3 populated, then pop, array.toString", ",1,2", x.toString());
	array[item++] = new TestCase( SECTION, "create array with 1-3 populated, then pop, get results", 3, z);

	var x = new Array();
	x[1] = 1;
	x.push (2, 3);
	array[item++] = new TestCase( SECTION, "create array with 1 populated, then push(2,3), array.toString", ",1,2,3", x.toString());

	var x = new Array();
	x[1] = 1;
	x[2] = 2;
	x[3] = 3;
	x[4] = 4;
	z = x.reverse();
	array[item++] = new TestCase( SECTION, "create array with 1-4 populated, then reverse, array.toString", "4,3,2,1,", x.toString());

	return ( array );
}
