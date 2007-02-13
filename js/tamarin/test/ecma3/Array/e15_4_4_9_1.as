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
 *  File Name:          e15_4_4_9.as
 *  ECMA Section:       15.4.4.9 Array.prototype.shift()
 *  Description:        Test Case for reverse function of Array Class.
 *			The first element of the array is removed from the array and returned.
 *
 *			1. Call the [[Get]] method of this object with argument "length".
 *			2. Call ToUint32(Result(1)).
 *			3. If Result(2) is not zero, go to step 6.
 *			4. Call the [[Put]] method of this object with arguments "length" and Result(2).
 *			5. Return undefined.
 *			6. Call the [[Get]] method of this object with argument 0.
 *			7. Let k be 1.
 *			8. If k equals Result(2), go to step 18.
 *			9. Call ToString(k).
 *			10. Call ToString(k-1).
 *			11. If this object has a property named by Result(9), go to step 12; but if this object has no property
 *			    named by Result(9), then go to step 15.
 *			12. Call the [[Get]] method of this object with argument Result(9).
 *			13. Call the [[Put]] method of this object with arguments Result(10) and Result(12).
 *			14. Go to step 16.
 *			15. Call the [[Delete]] method of this object with argument Result(10).
 *			16. Increase k by 1.
 *			17. Go to step 8.
 *			18. Call the [[Delete]] method of this object with argument ToString(Result(2)-1).
 *			19. Call the [[Put]] method of this object with arguments "length" and (Result(2)-1).
 *			20. Return Result(6).
 *
 *	Note:
 *	The shift function is intentionally generic; it does not require that its this value be an Array object.
 *	Therefore it can be transferred to other kinds of objects for use as a method. Whether the shift
 *	function can be applied successfully to a host object is implementation-dependent.

 *			This test case is for checking if the length of
 *			a non initailized array after shifting; is zero or not.
 *			Given by Werner Sharp (wsharp@macromedia.com)

 *
 *  Author: 		Gagneet Singh (gasingh@macromedia.com)
 *  Date:   		01/09/2005
 *
 */

var SECTION = "15.4.4.9";
var TITLE   = "Array.shift";

var VERSION = "ECMA_3";

startTest();

writeHeaderToLog( SECTION + " " + TITLE);


var testcases = getTestCases();

test();

function getTestCases() {
	var array = new Array();
	var item = 0;

	// Create an array from which we will shift an element.
	var MYARR = new Array();

	// As the array is not initialized with any elements, we should get a value of zero
	// for the length of the array.
	MYARR.shift();


	array[item++] = new TestCase( SECTION, "MYARR = []; MYARR.shift(); MYARR.length", 0, MYARR.length );


	return ( array );

}
