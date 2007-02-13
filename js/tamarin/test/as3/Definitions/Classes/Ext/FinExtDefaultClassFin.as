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


/**
 * The 'import' statements should be the first 
 * non-comment line in a file.
 *
 * These lines have to be commented out.
 * The compiler requires only the parent Class to be imported and not the Subclass folder.
 * Hence change in the import statements required.
 *  import Definitions.Classes.Simple;
 *  import Definitions.Classes.Ext.FinExtDefaultClassFin;
 */
import DefaultClass.*;


var SECTION = "Definitions\Ext";       			// provide a document reference (ie, ECMA section)
var VERSION = "AS 3.0";  				// Version of JavaScript or ECMA
var TITLE   = "Final Class Extends Default Class Final Methods";    	// Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                				// leave this alone

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

// Can't create an instance because it's not public
// Hence commenting out the lines where the class is initialized.
// var EXTDCLASS = new FinExtDefaultClassPub();
  
  
// Create an array variable which will define the test array to be used
// for the given output.
var arr = new Array(1, 2, 3);


// access final method from a default method of a sub class
AddTestCase( "*** Access default method from default method of sub class ***", 1, 1 );
AddTestCase( "subSetArray( arr ), subGetArray()", arr, ( subSetArray( arr ), subGetArray() ) );


// access final method from a dynamic method of a sub class
AddTestCase( "*** Acess default method from dynamic method of sub class ***", 1, 1 );
AddTestCase( "dynSubSetArray( arr ), dynSubGetArray()", arr, ( dynSubSetArray( arr ), dynSubGetArray()) );


// access final method from a public method of a sub class
AddTestCase( "*** Access default method from public method of sub class ***", 1, 1 );
AddTestCase( "pubSubSetArray( arr ), pubSubGetArray()", arr, ( pubSubSetArray( arr ), pubSubGetArray()) );


// access final method from a private method of a sub class
AddTestCase( "*** Access default method from private method of sub class ***", 1, 1 );
AddTestCase( "testPrivSubArray( arr )", arr, testPrivSubArray( arr ) );


// access final method from a static method of a sub class
AddTestCase( "*** Access default method from static method of sub class ***", 1, 1 );
AddTestCase( "*** Static Method cannot access any other methods except static methods of the parent class ***", 1, 1 );
// AddTestCase( "statSubSetArray( arr ), statSubGetArray()", arr, ( statSubSetArray( arr ), statSubGetArray() ) );


// access final method from a final method of a sub class
AddTestCase( "*** Access default method from final method of sub class ***", 1, 1 );
AddTestCase( "finSubSetArray( arr ), finSubGetArray()", arr, ( finSubSetArray( arr ), finSubGetArray() ) );


// access final method from a virtual method of a sub class
AddTestCase( "*** Access default method from virtual method of sub class ***", 1, 1 );
AddTestCase( "virSubSetArray( arr ), virSubSetArray()", arr, ( virSubSetArray( arr ), virSubGetArray() ) );


// access final method from a public dynamic method of a sub class
AddTestCase( "*** Acess default method from public dynamic method of sub class ***", 1, 1 );
AddTestCase( "pubDynSubSetArray( arr ), pubDynSubGetArray()", arr, ( pubDynSubSetArray( arr ), pubDynSubGetArray()) );


// access final method from a public static method of a sub class
AddTestCase( "*** Access default method from public static method of sub class ***", 1, 1 );
AddTestCase( "*** Static Method cannot access any other methods except static methods of the parent class ***", 1, 1 );
// AddTestCase( "pubStatSubSetArray( arr ), pubStatSubGetArray()", arr, ( pubStatSubSetArray( arr ), pubStatSubGetArray() ) );


// access final method from a public final method of a sub class
AddTestCase( "*** Access default method from public final method of sub class ***", 1, 1 );
AddTestCase( "pubFinSubSetArray( arr ), pubFinSubGetArray()", arr, ( pubFinSubSetArray( arr ), pubFinSubGetArray() ) );


// access final method from a public virtual method of a sub class
AddTestCase( "*** Access default method from public virtual method of sub class ***", 1, 1 );
AddTestCase( "pubVirSubSetArray( arr ), pubVirSubSetArray()", arr, ( pubVirSubSetArray( arr ), pubVirSubGetArray() ) );


// access final method from a final private method of a sub class
AddTestCase( "*** Access default method from final private method of sub class ***", 1, 1 );
AddTestCase( "testPrivFinSubArray( arr )", arr, testPrivFinSubArray( arr ) );


// access final method from a final static method of a sub class
AddTestCase( "*** Access default method from final static method of sub class ***", 1, 1 );
AddTestCase( "*** Static Method cannot access any other methods except static methods of the parent class ***", 1, 1 );
// AddTestCase( "finStatSubSetArray( arr ), finStatSubGetArray()", arr, ( finStatSubSetArray( arr ), finStatSubGetArray() ) );


// access final method from a private static method of a sub class
AddTestCase( "*** Access default method from private static method of sub class ***", 1, 1 );
AddTestCase( "*** Static Method cannot access any other methods except static methods of the parent class ***", 1, 1 );
// AddTestCase( "testPrivStatSubArray( arr )", arr, testPrivStatSubArray( arr ) );


// access final method from a private virtual method of a sub class
AddTestCase( "*** Access default method from private virtual method of sub class ***", 1, 1 );
AddTestCase( "testPrivVirSubArray( arr )", arr, testPrivVirSubArray( arr ) );


// access final property from outside the class
AddTestCase( "*** Access final from outside the class ***", 1, 1 );
AddTestCase( "finArray = arr", arr, (finArray = arr, finArray) );


// <TODO>  fill in the rest of the cases here

// access final property from outside the class
AddTestCase( "*** Access default property from outside the class ***", 1, 1 );
AddTestCase( "array = arr", arr, (array = arr, array) );


// access final property from a default method of a sub class
AddTestCase( "*** Access default property from default method of sub class ***", 1, 1 );
AddTestCase( "subSetDPArray( arr ), subGetDPArray()", arr, ( subSetDPArray( arr ), subGetDPArray() ) );


// access final property from a dynamic method of a sub class
AddTestCase( "*** Access default property from dynamic method of sub class ***", 1, 1 );
AddTestCase( "dynSubSetDPArray( arr ), dynSubGetDPArray()", arr, ( dynSubSetDPArray( arr ), dynSubGetDPArray() ) );


// access final property from a public method of a sub class
AddTestCase( "*** Access default property from public method of sub class ***", 1, 1 );
AddTestCase( "pubSubSetDPArray( arr ), pubSubGetDPArray()", arr, ( pubSubSetDPArray( arr ), pubSubGetDPArray() ) );


// access final property from a private method of a sub class
AddTestCase( "*** Access default property from private method of sub class ***", 1, 1 );
AddTestCase( "testPrivSubDPArray( arr )", arr, testPrivSubDPArray( arr ) );


// access final property from a static method of a sub class
AddTestCase( "*** Access default property from static method of sub class ***", 1, 1 );
AddTestCase( "statSubSetDPArray( arr ), statSubGetDPArray()", arr, ( statSubSetDPArray( arr ), statSubGetDPArray() ) );


// access final property from a final method of a sub class
AddTestCase( "*** Access default property from final method of sub class ***", 1, 1 );
AddTestCase( "finSubSetDPArray( arr ), finSubGetDPArray()", arr, ( finSubSetDPArray( arr ), finSubGetDPArray() ) );


// access final property from a private virtual method of a sub class
AddTestCase( "*** Access default property from private virtual method of sub class ***", 1, 1 );
AddTestCase( "virSubSetDPArray( arr ), virSubGetDPArray()", arr, ( virSubSetDPArray( arr ), virSubGetDPArray() ) );


// access final property from a public static method of a sub class
AddTestCase( "*** Access default property from public static method of sub class ***", 1, 1 );
AddTestCase( "pubStatSubSetDPArray( arr ), pubStatSubGetDPArray()", arr, ( pubStatSubSetDPArray( arr ), pubStatSubGetDPArray() ) );


// access final property from a private static method of a sub class
AddTestCase( "*** Access default property from private static method of sub class ***", 1, 1 );
AddTestCase( "testPrivStatSubDPArray( arr )", arr, testPrivStatSubDPArray( arr ) );


test();       		// Leave this function alone.
			// This function is for executing the test case and then
			// displaying the result on to the console or the LOG file.
