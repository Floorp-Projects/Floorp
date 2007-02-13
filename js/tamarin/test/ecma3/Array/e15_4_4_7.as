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
 *  File Name:          e15_4_4_7.as
 *  ECMA Section:       15.4.4.7 Array.prototype.push()
 *  Description:        Test Case for push function of Array Class.
 *			The arguments are appended to the end of the array,
 *			in the order in which they appear. The new length of
 *			the array is returned as a result of the call.

 *
 *  Author: 		Gagneet Singh (gasingh@macromedia.com)
 *  Date:   		01/09/2005
 *
 */

var SECTION = "15.4.4.7";
var TITLE   = "Array.push";

var VERSION = "ECMA_3";

startTest();

writeHeaderToLog( SECTION + " " + TITLE);


var testcases = getTestCases();

test();

function getTestCases() {
	var array = new Array();
	var item = 0;

        var MYEMPTYARR:Array= new Array();

        var EMPTYARRLENGTH = 0;

        array[item++] = new TestCase( SECTION, "MYEMPTYARR = new Array(); MYEMPTYARR.push();", EMPTYARRLENGTH, MYEMPTYARR.push() );

        var MYEMPTYARRLENGTHAFTERPUSH = 1;

        array[item++] = new TestCase( SECTION, "MYEMPTYARR = new Array(); MYEMPTYARR.push(2);", MYEMPTYARRLENGTHAFTERPUSH, MYEMPTYARR.push(2) );

        array[item++] = new TestCase( SECTION, "MYEMPTYARR = new Array(); MYEMPTYARR[0];", 2, MYEMPTYARR[0] );


        var MYARR = new Array( 1, 4 );

	var MYVAR = 2;
	var ARRLENGTH = 3;

        array[item++] = new TestCase( SECTION, "MYARR = new Array(); MYARR.push();", 2, MYARR.push() );


	array[item++] = new TestCase( SECTION, "MYARR = new Array(); MYARR.push(2);", ARRLENGTH, MYARR.push(MYVAR) );

        //push function is intentionally generic.  It does not require its this value to be         //an array object

        var obj = new Object();
        obj.push = Array.prototype.push;
        obj.length = 4
        obj[0]=0;
        obj[1]=1;
        obj[2]=2;
        obj[3]=3;

        array[item++] = new TestCase( SECTION, "var obj = new Object(); obj.push(4);", 5, obj.push(4) );

        var MYBIGARR = []

        for (var i=0;i<101;i++){
            MYBIGARR[MYBIGARR.length] = i;
        }

        array[item++] = new TestCase( SECTION, "var MYBIGARR[i] = i; MYBIGARR.push(101);", 102, MYBIGARR.push(101) );
	return ( array );

}
