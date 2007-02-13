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
 *  File Name:          e15_4_4_8.as
 *  ECMA Section:       15.4.4.8 Array.prototype.reverse()
 *  Description:        Test Case for reverse function of Array Class.
 *			The elements of the array are rearranged so as to reverse their order.
 *			The object is returned as the result of the call.
 *			1. Call the [[Get]] method of this object with argument "length".
 *			2. Call ToUint32(Result(1)).
 *			3. Compute floor(Result(2)/2).
 *			4. Let k be 0.
 *			5. If k equals Result(3), return this object.
 *			6. Compute Result(2)-k-1.
 *			7. Call ToString(k).
 *			8. Call ToString(Result(6)).
 *			9. Call the [[Get]] method of this object with argument Result(7).
 *			10. Call the [[Get]] method of this object with argument Result(8).
 *			11. If this object does not have a property named by Result(8), go to step 19.
 *			12. If this object does not have a property named by Result(7), go to step 16.
 *			13. Call the [[Put]] method of this object with arguments Result(7) and Result(10).
 *			14. Call the [[Put]] method of this object with arguments Result(8) and Result(9).
 *			15. Go to step 25.
 *			16. Call the [[Put]] method of this object with arguments Result(7) and Result(10).
 *			17. Call the [[Delete]] method on this object, providing Result(8) as the name of the property to
 *			    delete.
 *			18. Go to step 25.
 *			19. If this object does not have a property named by Result(7), go to step 23.
 *			20. Call the [[Delete]] method on this object, providing Result(7) as the name of the property to
 *			    delete..
 *			21. Call the [[Put]] method of this object with arguments Result(8) and Result(9).
 *			22. Go to step 25.
 *			23. Call the [[Delete]] method on this object, providing Result(7) as the name of the property to
 *			    delete.
 *			24. Call the [[Delete]] method on this object, providing Result(8) as the name of the property to
 *			    delete.
 *			25. Increase k by 1.
 *			26. Go to step 5.
 *
 *	Note:
 *	The reverse function is intentionally generic; it does not require that its this value be an Array
 *	object. Therefore, it can be transferred to other kinds of objects for use as a method. Whether the
 *	reverse function can be applied successfully to a host object is implementation-dependent.

 *
 *  Author: 		Gagneet Singh (gasingh@macromedia.com)
 *  Date:   		01/09/2005
 *
 */

var SECTION = "15.4.4.8";
var TITLE   = "Array.reverse";

var VERSION = "ECMA_3";

startTest();

writeHeaderToLog( SECTION + " " + TITLE);


var testcases = getTestCases();

test();

function getTestCases() {
	var array = new Array();
	var item = 0;

	// Create an array from which we will reverse the elements of the array.
	var MYARR = new Array( 2, 1, 8, 6 );
	var EXPARR = [ 6, 8, 1, 2 ];


	MYARR.reverse();


	for (var MYVAR = 0; ( MYVAR < MYARR.length ); MYVAR++)
	{
		array[item++] = new TestCase( SECTION, "MYARR = [2,1,8,6]; MYARR.reverse();", EXPARR[MYVAR], MYARR[MYVAR] );
	}


	return ( array );

}
