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
 *  File Name:          e15_4_4_6.as
 *  ECMA Section:       15.4.4.6 Array.prototype.pop()
 *  Description:        Test Case for pop function of Array Class.
 *			The last element is removed from the array and returned.
 *			1.  Call the [[Get]] method of this object with argument "length".
 *			2.  Call ToUint32(Result(1)).
 *			3.  If Result(2) is not zero, go to step 6.
 *			4.  Call the [[Put]] method of this object with arguments "length" and Result(2).
 *			5.  Return undefined.
 *			6.  Call ToString(Result(2)-1).
 *			7.  Call the [[Get]] method of this object with argument Result(6).
 *			8.  Call the [[Delete]] method of this object with argument Result(6).
 *			9.  Call the [[Put]] method of this object with arguments "length" and (Result(2)-1).
 *			10. Return Result(7).

 *	Notes:
 *	The pop function is intentionally generic; it does not require that its 'this' value
 *	be an Array object.
 *	Therefore it can be transferred to other kinds of objects for use as a method.
 *	Whether the pop function can be applied successfully to a host object is implementation-dependent.

 *
 *  Author: 		Gagneet Singh (gasingh@macromedia.com)
 *  Date:   		01/09/2005
 *  Modified By:        Subha Subramanian
 *  Date:               01/04/2006(Added test cases for calling pop() method on an array  *  that is empty and test case for transferring pop method to other object which is not an  *  array
 *
 */

var SECTION = "15.4.4.6";
var TITLE   = "Array.pop";

var VERSION = "ECMA_3";

startTest();

writeHeaderToLog( SECTION + " " + TITLE);


var testcases = getTestCases();

test();

function getTestCases() {
	var array = new Array();
	var item = 0;

        var MYEMPTYARRAY = new Array();
        array[item++] = new TestCase( SECTION, "MYEMPTYARRAY = new Array(); MYEMPTYARRAY.pop();", undefined, MYEMPTYARRAY.pop());

	// Create an array from which we will pop an element.
	var MYARR = new Array( 2, 1, 8, 6 );
	var EXPARR = [ 2, 1, 8 ];


	var EXP_RESULT = MYARR.pop();

	array[item++] = new TestCase( SECTION, "MYARR = [2,1,8,6]; MYARR.pop();", 6, EXP_RESULT );

	for (var MYVAR = 0; ( MYVAR < MYARR.length ); MYVAR++)
	{
		array[item++] = new TestCase( SECTION, "MYARR = [2,1,8,6]; MYARR.pop();", EXPARR[MYVAR], MYARR[MYVAR] );
	}

       //pop method is generic so can be transferred to other types of objects
        var obj = new Object();
        obj.pop = Array.prototype.pop;
        obj.length = 4;
        obj[0] = 2;
        obj[1] = 1;
        obj[2] = 8;
        obj[3] = 6;

        var EXP_OBJRESULT = obj.pop();

        array[item++] = new TestCase( SECTION, "obj.pop()", 6, EXP_OBJRESULT );

        for (var MYVAR1 = 0; ( MYVAR1 < obj.length ); MYVAR1++)
	{
		array[item++] = new TestCase( SECTION, "obj.pop()", EXPARR[MYVAR1], obj[MYVAR1] );
	}



	return ( array );

}
