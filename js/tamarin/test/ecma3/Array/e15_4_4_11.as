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
 *  File Name:          e15_4_4_11.as
 *  ECMA Section:       15.4.4.11 Array.prototype.sort()
 *  Description:        Test Case for sort function of Array Class.
 *			The elements of the array are sorted.
 *			The sort is not necessary stable (this is, elements
 *			that compare equal do not necessarily remain in their
 *			original order.

 *
 *  Author: 		Gagneet Singh (gasingh@macromedia.com)
 *  Date:   		01/09/2005
 *
 */

var SECTION = "15.4.4.11";
var TITLE   = "Array.sort";

var VERSION = "ECMA_3";

startTest();

writeHeaderToLog( SECTION + " " + TITLE);


var testcases = getTestCases();

test();

function getTestCases() {
	var array = new Array();
	var item = 0;

	// Check for only numeric values.
	var MYARR = new Array( 2, 1, 8, 6 );
	var EXPARR = [1,2,6,8];

	MYARR.sort()

	for (var MYVAR = 0; ( MYVAR < MYARR.length ); MYVAR++)
	{
		array[item++] = new TestCase( SECTION, "MYARR = [2,1,8,6]; MYARR.sort()", EXPARR[MYVAR], MYARR[MYVAR] );
	}


	// Check for only alpha-numeric values.
	var MYARR = new Array( 'a', 'd', 'Z', 'f', 'M' );
	var EXPARR = ['M', 'Z', 'a', 'd', 'f'];

	MYARR.sort()

	for (var MYVAR = 0; ( MYVAR < MYARR.length ); MYVAR++)
	{
		array[item++] = new TestCase( SECTION, "MYARR = ['a','d','Z','f','M']; MYARR.sort()", EXPARR[MYVAR], MYARR[MYVAR] );
	}


	// Check for numeric and alpha-numeric values.
	var MYARR = new Array( 2, 1, 'M', 'y', 'X', 66, 104 );
	var EXPARR = [1, 104, 2, 66, 'M', 'X', 'y'];

	MYARR.sort()

	for (var MYVAR = 0; ( MYVAR < MYARR.length ); MYVAR++)
	{
		array[item++] = new TestCase( SECTION, "MYARR = [2, 1, 'M', 'X', 'y', 66, 104]; MYARR.sort()", EXPARR[MYVAR], MYARR[MYVAR] );
	}


	return ( array );

}
