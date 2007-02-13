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
 *  File Name:          e15_4_4_12.as
 *  ECMA Section:       15.4.4.10 Array.prototype.splice()
 *  Description:        Test Case for push function of Array Class.
 *			The method takes 2 arguments - start and end.
 *			It returns an array containing the elements of
 *			the array from element start upto but not including
 *			the element end.
 *			If end is undefined then end is the last element.
 *			If start is negative, it is treated as (length+start)
 *			where length is the length of the array.

 *
 *  Author: 		Gagneet Singh (gasingh@macromedia.com)
 *  Date:   		01/09/2005
 *
 */

var SECTION = "15.4.4.12";
var TITLE   = "Array.splice";

var VERSION = "ECMA_3";

startTest();

writeHeaderToLog( SECTION + " " + TITLE);


var testcases = getTestCases();

test();

function getTestCases() {
	var array = new Array();
	var item = 0;

	var MYARR = new Array();
	MYARR = [0, 2, 3, 4, 5];

	var RESULTARR = MYARR.splice(1);

	var EXPCTARR = [ 2, 3, 4, 5 ]

	for (var MYVAR = 0; ( MYVAR < RESULTARR.length ); MYVAR++)
	{
		array[item++] = new TestCase( SECTION, "MYARR = new Array( 0, 2, 3, 4, 5 ); MYARR.splice(1);", EXPCTARR[MYVAR], RESULTARR[MYVAR] );
	}


	return ( array );

}
