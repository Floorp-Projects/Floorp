/* ***** BEGIN LICENSE BLOCK ***** 
 Version: MPL 1.1/GPL 2.0/LGPL 2.1 

The contents of this file are subject to the Mozilla Public License Version 1.1 (the 
"License"); you may not use this file except in compliance with the License. You may obtain 
a copy of the License at http://www.mozilla.org/MPL/ 

Software distributed under the License is distributed on an "AS IS" basis, WITHOUT 
WARRANTY OF ANY KIND, either express or implied. See the License for the specific 
language governing rights and limitations under the License. 

The Original Code is [Open Source Virtual Machine.] 

The Initial Developer of the Original Code is Adobe System Incorporated.  Portions created 
by the Initial Developer are Copyright (C)[ 2005-2006 ] Adobe Systems Incorporated. All Rights 
Reserved. 

Contributor(s): Adobe AS3 Team

Alternatively, the contents of this file may be used under the terms of either the GNU 
General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public 
License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the 
LGPL are applicable instead of those above. If you wish to allow use of your version of this 
file only under the terms of either the GPL or the LGPL, and not to allow others to use your 
version of this file under the terms of the MPL, indicate your decision by deleting provisions 
above and replace them with the notice and other provisions required by the GPL or the 
LGPL. If you do not delete the provisions above, a recipient may use your version of this file 
under the terms of any one of the MPL, the GPL or the LGPL. 

 ***** END LICENSE BLOCK ***** */
/*
 *  File Name:          e15_4_4_13.as
 *  ECMA Section:       15.4.4.13 Array.unshift()
 *  Description:        Test Case for unshift function of Array Class.
 *			The arguments given to unshift are prepended to the start
 *			of the array, such that their order within the array is the
 *			same as the order in which they appear in the argument list.

 *
 *  Author: 		Gagneet Singh (gasingh@macromedia.com)
 *  Date:   		01/09/2005
 *  Modified By:        Subha Subramanian
 *  Date:               01/05/2006
 *  Details:            Added test cases to test the length of the unshift method, unshift    *                      without any parameters and to test that the unshift can be
 *                      transferred to other objects for use as method
 */

var SECTION = "15.4.4.13";
var TITLE   = "Array.unshift";

var VERSION = "ECMA_3";

startTest();

writeHeaderToLog( SECTION + " " + TITLE);


var testcases = getTestCases();

test();

function getTestCases() {
	var array = new Array();
        var myobj = new Object();
	var item = 0;

       array[item++] = new TestCase( SECTION, "Array.prototype.unshift.length;",0, Array.prototype.unshift.length);

        var MYEMPTYARR = new Array();

        array[item++] = new TestCase( SECTION, "MYEMPTYARR = new Array();MYEMPTYARR.unshift();", 0, MYEMPTYARR.unshift() );
        array[item++] = new TestCase( SECTION, "MYEMPTYARR = new Array();MYEMPTYARR.unshift(0);", 1, MYEMPTYARR.unshift(0) );


        var MYARR = new Array();
	MYARR.push( 1, 2 );


       var MYVAR:Number = 3

	array[item++] = new TestCase( SECTION, "MYARR = new Array(); MYARR.push(1, 2); MYARR.unshift(0);", MYVAR, MYARR.unshift(0) );

        for (var i = 0;i<MYARR.length; i++){

        array[item++] = new TestCase( SECTION, "MYARR = new Array(); MYARR.push(1, 2); MYARR.unshift(0);MYARR", i, MYARR[i] );

        }
        //unshift method can be transferred to other objects for use as method

        myobj.unshift= Array.prototype.unshift;
        myobj.length = 3
        myobj[0] = 3;
        myobj[1] = 4;
        myobj[2] = 5;


        array[item++] = new TestCase( SECTION, "myobj = new Object(); myobj.unshift= Array.prototype.unshift;myobj.unshift(0,1,2);", 6, myobj.unshift(0,1,2) );

        for (var i=0; i<6; i++){
        array[item++] = new TestCase( SECTION, "myobj = new Object(); myobj.unshift= Array.prototype.unshift; myobj.unshift(0,1,2);",i, myobj[i] );
        }
	return ( array );

}
