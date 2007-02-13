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
 * Portions created by the Initial Developer are Copyright (C) 2004-2006
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



/** These lines have to be commented out.
 *  The compiler requires only the parent Class to be imported and not the Subclass folder.
 *  Hence change in the import statements required.
 * import Definitions.Classes.InternalClass;
 * import Definitions.Classes.Ext.DynExtInternalClass;
 */
import InternalClass.*;

var SECTION = "Definitions";       			// provide a document reference (ie, ECMA section)
var VERSION = "AS 3.0";  				// Version of JavaScript or ECMA
var TITLE   = "Dynamic Class Extends Default Class";    // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone

/**
 * Calls to AddTestCase here. AddTestCase is a function that is defined
 * in shell.js and takes three arguments:
 * - a string representation of what is being tested
 * - the expected result
 * - the actual result
 *
 * For example, a test might look like this:
 *
 * var helloWorld = "Hello World";
 *
 * AddTestCase(
 * "var helloWorld = 'Hello World'",   // description of the test
 *  "Hello World",                     // expected result
 *  helloWorld );                      // actual result
 *
 */


// *******************************************
//  access default method from
//  outside of class
// *******************************************

var DYNEXTDCLASS = new DynExtInternalClass();
var arr = new Array(10, 15, 20, 25, 30);

AddTestCase( "*** Access Default Method from Default method of sub class  ***", 1, 1 );
AddTestCase( "DYNEXTDCLASS.testSubGetSetArray(arr)", arr, (DYNEXTDCLASS.testSubGetSetArray(arr)) );


// ********************************************
// access default method from a public
// method of a sub class
//
// ********************************************
var arr = new Array(1, 5);
DYNEXTDCLASS = new DynExtInternalClass();
AddTestCase( "DYNEXTDCLASS.pubSubSetArray(arr), DYNEXTDCLASS.pubSubGetArray()", arr, (DYNEXTDCLASS.pubSubSetArray(arr), DYNEXTDCLASS.pubSubGetArray()) );


// ********************************************
// access default method from a private
// method of a sub class
//
// ********************************************
var arr = new Array(2, 4, 6);
DYNEXTDCLASS = new DynExtInternalClass();
AddTestCase( "DYNEXTDCLASS.testPrivSubArray(arr)", arr, DYNEXTDCLASS.testPrivSubArray(arr) );

// ********************************************
// access default method from a final
// method of a sub class
// ********************************************

DYNEXTDCLASS = new DynExtInternalClass();
AddTestCase( "access 'default' method from 'final' method of sub class", arr, (DYNEXTDCLASS.testFinSubArray(arr)) );

// ********************************************
// access default method from a static 
// method of a sub class
// ********************************************

DYNEXTDCLASS = new DynExtInternalClass();
var thisError = "no Exception thrown";
try{
	DYNEXTDCLASS.testStatSubArray(arr);
} catch (e) {
	thisError = e.toString();
} finally {
	AddTestCase( "access 'default' method from 'static' method of sub class", 
				 "ReferenceError: Error #1065",
				 referenceError( thisError ) );
}
// ********************************************
// access default method from a static
// method of a sub class
//
// ********************************************
/*
var arr = new Array(1, 5);
DYNEXTDCLASS = new DynExtInternalClass();
AddTestCase( "*** Access default method from static method of sub class ***", 1, 1 );
AddTestCase( "DYNEXTDCLASS.testStatSubArray(arr)", arr, (DYNEXTDCLASS.testStatSubArray(arr)) );
*/

// ********************************************
// access default method from a public 
// final method of a sub class
// ********************************************

arr = new Array( 1, 2, 3 );
DYNEXTDCLASS = new DynExtInternalClass();
AddTestCase( "access 'default' method from 'public final' method of sub class", arr, 
		     (DYNEXTDCLASS.pubFinSubSetArray(arr), DYNEXTDCLASS.pubFinSubGetArray()) );

// ********************************************
// access default method from a final 
// private method of a sub class
// ********************************************

arr = new Array( 4, 5 );
DYNEXTDCLASS = new DynExtInternalClass();
AddTestCase( "access 'default' method from 'private final' method of sub class", arr, (DYNEXTDCLASS.testPrivFinSubArray(arr)) );

// ********************************************
// access default method from a private 
// method of a sub class
// ********************************************

arr = new Array( 6, 7 );
DYNEXTDCLASS = new DynExtInternalClass();
AddTestCase( "access 'default' method from 'private' method of sub class", arr, DYNEXTDCLASS.testPrivSubArray(arr) );

// ********************************************
// access default method from a virtual 
// method of a sub class
// ********************************************

AddTestCase( "access 'default' method from 'virtual' method of sub class", arr, 
			  DYNEXTDCLASS.testVirtSubArray(arr) );

// ********************************************
// access default method from a virtual 
// public method of a sub class
// ********************************************

AddTestCase( "access 'default' method from 'public virtual' method of sub class", arr, 
		     (DYNEXTDCLASS.pubVirtSubSetArray(arr), DYNEXTDCLASS.pubVirtSubGetArray()) );

// ********************************************
// access default method from a virtual 
// private method of a sub class
// ********************************************

AddTestCase( "access 'default' method from 'private virtual' method of sub class", arr, 
			  DYNEXTDCLASS.testPrivVirtSubArray(arr) );



/* Access properties of parent class */ 

// ********************************************
// access default property from 
// default method in sub class
// ********************************************

DYNEXTDCLASS = new DynExtInternalClass();
AddTestCase( "access 'default' property from 'default' method of sub class", arr, 
				(DYNEXTDCLASS.testSubGetSetDPArray(arr)) );

// ********************************************
// access default property from 
// final method in sub class
// ********************************************

DYNEXTDCLASS = new DynExtInternalClass();
AddTestCase( "access 'default' property from 'final' method of sub class", arr, 
				(DYNEXTDCLASS.testFinSubDPArray(arr)) );

// ********************************************
// access default property from 
// virtual method in sub class
// ********************************************

DYNEXTDCLASS = new DynExtInternalClass();
AddTestCase( "access 'default' property from 'virtual' method of sub class", arr, 
				(DYNEXTDCLASS.testVirtSubDPArray(arr)) );

// ********************************************
// access default property from 
// public method in sub class
// ********************************************

DYNEXTDCLASS = new DynExtInternalClass();
AddTestCase( "access 'default' property from 'public' method of sub class", arr, 
				(DYNEXTDCLASS.pubSubSetDPArray(arr), DYNEXTDCLASS.pubSubGetDPArray()) );

// ********************************************
// access default property from 
// private method in sub class
// ********************************************

DYNEXTDCLASS = new DynExtInternalClass();
AddTestCase( "access 'default' property from 'private' method of sub class", arr, 
			 (DYNEXTDCLASS.testPrivSubDPArray(arr)) );

// ********************************************
// access default property from 
// public final method in sub class
// ********************************************

DYNEXTDCLASS = new DynExtInternalClass();
AddTestCase( "access 'default' property from 'public final' method of sub class", arr, 
			 (DYNEXTDCLASS.pubFinSubSetDPArray(arr), DYNEXTDCLASS.pubFinSubGetDPArray()) );

// ********************************************
// access default property from 
// public virtual method in sub class
// ********************************************

DYNEXTDCLASS = new DynExtInternalClass();
AddTestCase( "access 'default' property from 'public virtual' method of sub class", arr, 
			 (DYNEXTDCLASS.pubVirtSubSetDPArray(arr), DYNEXTDCLASS.pubVirtSubGetDPArray()) );

// ********************************************
// access default property from 
// private final method in sub class
// ********************************************

DYNEXTDCLASS = new DynExtInternalClass();
AddTestCase( "access 'default' property from 'private final' method of sub class", arr, 
			 (DYNEXTDCLASS.testPrivFinSubDPArray(arr)) );

// ********************************************
// access default property from 
// private virtual method in sub class
// ********************************************

DYNEXTDCLASS = new DynExtInternalClass();
AddTestCase( "access 'default' property from 'private virtual' method of sub class", arr, 
			 (DYNEXTDCLASS.testPrivVirtSubDPArray(arr)) );

test();       		// Leave this function alone.
			// This function is for executing the test case and then
			// displaying the result on to the console or the LOG file.
