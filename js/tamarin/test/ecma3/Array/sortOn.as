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
 *  File Name:          sortOn.as
 *  ECMA Section:       15.4.4.3 Array.sortOn()
 *  Description:        Test Case for sortOn function of Array Class.
 * 			Here three objects have been created and using the
 * 			sortOn() function, we are sorting the fields in the
 * 			Object.
 *
 *  Author: 		Gagneet Singh (gasingh@macromedia.com)
 *  Date:   		01/09/2005
 *  Modified By:        Subha Subramanian
 *  Date:               01/05/2006
 *  Details:            Added more tests to test sortOn method and added tests to test        *  sortOn method's  changed sorting behavior with Array class constants
 */

var SECTION = "sortOn";
var TITLE   = "Array.sortOn";

var VERSION = "ECMA_3";

startTest();

writeHeaderToLog( SECTION + " " + TITLE);


var testcases = getTestCases();

test();

function getTestCases() {
	var array = new Array();
	var item = 0;

	// Create three objects to hold
	var OBJ1	= new Object();
	OBJ1.name	= "Chris";
	OBJ1.city	= "Dallas";
	OBJ1.zip	= 72501;

	var OBJ2	= new Object();
	OBJ2.name	= "Mike";
	OBJ2.city	= "Newton";
	OBJ2.zip	= 68144;

	var OBJ3	= new Object();
	OBJ3.name	= "Greg";
	OBJ3.city	= "San Francisco";
	OBJ3.zip	= 94103;


	// Create an array to hold the three objects contents in an array object.
	var MYARRAY = new Array();

	// Push the objects created into the array created.
	MYARRAY.push(OBJ1);
	MYARRAY.push(OBJ2);
	MYARRAY.push(OBJ3);

	// Array to hold the output string as they were before.
	var EXPECT_VAR = new Array();

	// Output the current state of the array
	for (var SORTVAR = 0; ( SORTVAR < MYARRAY.length ); SORTVAR++)
	{
	 	EXPECT_VAR[SORTVAR] = (MYARRAY[SORTVAR].city);
	}


	// Sort the array on the City field.
	MYARRAY.sortOn("city");


	// Output the result of the Sort Operation.
	for (var MYVAR = 0; ( MYVAR < MYARRAY.length ); MYVAR++)
	{
		array[item++] = new TestCase( SECTION, "MYARR.sortOn(city)",  EXPECT_VAR[MYVAR],  MYARRAY[MYVAR].city );
	}
        var RESULT_ARRAY = new Array();
        for (var SORTVAR = 0; ( SORTVAR < MYARRAY.length ); SORTVAR++)
	{
	 	EXPECT_VAR[SORTVAR] = (MYARRAY[SORTVAR].name);

	}
        RESULT_ARRAY = EXPECT_VAR.sort();
        // Sort the array on the name field.
	MYARRAY.sortOn("name");


        // Output the result of the Sort Operation.
	for (var MYVAR = 0; ( MYVAR < MYARRAY.length ); MYVAR++)
	{
		array[item++] = new TestCase( SECTION, "MYARR.sortOn(name)",  RESULT_ARRAY[MYVAR],  MYARRAY[MYVAR].name );
	}

        for (var SORTVAR = 0; ( SORTVAR < MYARRAY.length ); SORTVAR++)
	{
	 	EXPECT_VAR[SORTVAR] = (MYARRAY[SORTVAR].zip);

	}
        RESULT_ARRAY = EXPECT_VAR.sort();
        // Sort the array on the zip field.
	MYARRAY.sortOn("zip");


        // Output the result of the Sort Operation.
	for (var MYVAR = 0; ( MYVAR < MYARRAY.length ); MYVAR++)
	{
		array[item++] = new TestCase( SECTION, "MYARR.sortOn(zip)",  RESULT_ARRAY[MYVAR],  MYARRAY[MYVAR].zip );
	}

        //Using constants to change the sorting behavior



        for (var SORTVAR = 0; ( SORTVAR < MYARRAY.length ); SORTVAR++)
	{
	 	EXPECT_VAR[SORTVAR] = (MYARRAY[SORTVAR].zip);

	}
        RESULT_ARRAY = EXPECT_VAR.sort(Array.NUMERIC);
        // Sort the array on the name field.
	MYARRAY.sortOn("zip",Array.NUMERIC);


        // Output the result of the Sort Operation.
	for (var MYVAR = 0; ( MYVAR < MYARRAY.length ); MYVAR++)
	{
		array[item++] = new TestCase( SECTION, "MYARR.sortOn(zip,Array.NUMERIC)",  RESULT_ARRAY[MYVAR],  MYARRAY[MYVAR].zip );
	}


        var users:Array = [{name:"Bob",age:3},{name:"barb",age:35},{name:"abcd",age:3},{name:"catchy",age:4}]

        for (var SORTVAR = 0; ( SORTVAR < users.length ); SORTVAR++)
	{
	 	EXPECT_VAR[SORTVAR] = (users[SORTVAR].name);

	}
        RESULT_ARRAY = EXPECT_VAR.sort(Array.CASEINSENSITIVE);
        // Sort the array on the name field.
	users.sortOn("name",Array.CASEINSENSITIVE);


        // Output the result of the Sort Operation.
	for (var MYVAR = 0; ( MYVAR < users.length ); MYVAR++)
	{
		array[item++] = new TestCase( SECTION, "users.sortOn(name,Array.CASEINSENSITIVE)",  RESULT_ARRAY[MYVAR],  users[MYVAR].name );
	}
        RESULT_ARRAY = EXPECT_VAR.sort(Array.CASEINSENSITIVE|Array.DESCENDING);
        // Sort the array on the name field.
	users.sortOn("name",Array.CASEINSENSITIVE|Array.DESCENDING);


        // Output the result of the Sort Operation.
	for (var MYVAR = 0; ( MYVAR < users.length ); MYVAR++)
	{
		array[item++] = new TestCase( SECTION, "users.sortOn(name,Array.CASEINSENSITIVE|Array.DESCENDING)",  RESULT_ARRAY[MYVAR],  users[MYVAR].name );
	}

        for (var SORTVAR = 0; ( SORTVAR < users.length ); SORTVAR++)
	{
	 	EXPECT_VAR[SORTVAR] = (users[SORTVAR].age);

	}
        RESULT_ARRAY = EXPECT_VAR.sort();
        // Sort the array on the age field.
	users.sortOn("age");


        // Output the result of the Sort Operation.
	for (var MYVAR = 0; ( MYVAR < users.length ); MYVAR++)
	{
		array[item++] = new TestCase( SECTION, "users.sortOn(age)",  RESULT_ARRAY[MYVAR],  users[MYVAR].age );
	}

        RESULT_ARRAY = EXPECT_VAR.sort(Array.NUMERIC);
        // Sort the array on the age field.
	users.sortOn("age",Array.NUMERIC);


        // Output the result of the Sort Operation.
	for (var MYVAR = 0; ( MYVAR < users.length ); MYVAR++)
	{
		array[item++] = new TestCase( SECTION, "users.sortOn(age,Array.NUMERIC)",  RESULT_ARRAY[MYVAR],  users[MYVAR].age );
	}

        RESULT_ARRAY = EXPECT_VAR.sort(Array.DESCENDING|Array.NUMERIC);
        // Sort the array on the age field.
	users.sortOn("age",Array.DESCENDING|Array.NUMERIC);


        // Output the result of the Sort Operation.
	for (var MYVAR = 0; ( MYVAR < users.length ); MYVAR++)
	{
		array[item++] = new TestCase( SECTION, "users.sortOn(age,Array.DESCENDING|Array.NUMERIC)",  RESULT_ARRAY[MYVAR],  users[MYVAR].age );
	}



	return ( array );

}
