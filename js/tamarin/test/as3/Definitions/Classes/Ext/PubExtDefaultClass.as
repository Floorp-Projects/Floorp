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

package DefaultClass{
import DefaultClass.*;

var SECTION = "Definitions";           // provide a document reference (ie, ECMA section)
var VERSION = "AS3";                   // Version of JavaScript or ECMA
var TITLE   = "Extend Default Class";  // Provide ECMA section title or a description
//var BUGNUMBER = "";

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

// ************************************
// public class extends <empty> class
// ************************************

var arr = new Array(1,2,3);

// ********************************************
// access default method from outside of class
// ********************************************

// this should give a compiler error
PUBEXTDCLASS = new PubExtDefaultClass();

//We cannot access the default method from outside the class
//AddTestCase( "PUBEXTDCLASS.setArray(arr), PUBEXTDCLASS.getArray()", arr, (PUBEXTDCLASS.setArray(arr), PUBEXTDCLASS.getArray()) );


// ********************************************
// access default method from a default
// method of a sub class
//
// ********************************************

PUBEXTDCLASS = new PubExtDefaultClass();
AddTestCase( "*** Access default method from default method of sub class ***", 1, 1 );
AddTestCase( "PUBEXTDCLASS.subSetArray(arr), PUBEXTDCLASS.subGetArray()", arr, PUBEXTDCLASS.testSubGetSetArray(arr) );

// <TODO>  fill in the rest of the cases here


// ********************************************
// access default method from a public
// method of a sub class
//
// ********************************************

PUBEXTDCLASS = new PubExtDefaultClass();
AddTestCase( "*** Access default method from public method of sub class ***", 1, 1 );
AddTestCase( "PUBEXTDCLASS.pubSubSetArray(arr), PUBEXTDCLASS.pubSubGetArray()", arr, (PUBEXTDCLASS.pubSubSetArray(arr), PUBEXTDCLASS.pubSubGetArray()) );

// <TODO>  fill in the rest of the cases here

// ********************************************
// access default method from a private
// method of a sub class
//
// ********************************************

PUBEXTDCLASS = new PubExtDefaultClass();
AddTestCase( "*** Access default method from private method of sub class ***", 1, 1 );
AddTestCase( "PUBEXTDCLASS.testPrivSubArray(arr)", arr, PUBEXTDCLASS.testPrivSubArray(arr) );

// <TODO>  fill in the rest of the cases here

// ********************************************
// access default method from a final
// method of a sub class
//
// ********************************************

PUBEXTDCLASS = new PubExtDefaultClass();
AddTestCase( "*** Access default method from default method of sub class ***", 1, 1 );
AddTestCase( "PUBEXTDCLASS.finSubSetArray(arr), PUBEXTDCLASS.finSubGetArray()", arr, PUBEXTDCLASS.testFinSubArray(arr) );

// ********************************************
// access default property from outside
// the class
// ********************************************

PUBEXTDCLASS = new PubExtDefaultClass();
AddTestCase( "*** Access default property from outside the class ***", 1, 1 );
//AddTestCase( "PUBEXTDCLASS.array = arr", arr, (PUBEXTDCLASS.array = arr, PUBEXTDCLASS.array) );

// <TODO>  fill in the rest of the cases here

// ********************************************
// access default property from
// default method in sub class
// ********************************************

PUBEXTDCLASS = new PubExtDefaultClass();
AddTestCase( "*** Access default property from method in sub class ***", 1, 1 );
AddTestCase( "PUBEXTDCLASS.subSetDPArray(arr), PUBEXTDCLASS.subGetDPArray()", arr, PUBEXTDCLASS.testSubGetSetDPArray(arr) );

// <TODO>  fill in the rest of the cases here

// ********************************************
// access default property from
// public method in sub class
// ********************************************

PUBEXTDCLASS = new PubExtDefaultClass();
AddTestCase( "*** Access default property from public method in sub class ***", 1, 1 );
AddTestCase( "PUBEXTDCLASS.pubSubSetDPArray(arr), PUBEXTDCLASS.pubSubGetDPArray()", arr, (PUBEXTDCLASS.pubSubSetDPArray(arr), PUBEXTDCLASS.pubSubGetDPArray()) );

// <TODO>  fill in the rest of the cases here

// ********************************************
// access default property from
// private method in sub class
// ********************************************

PUBEXTDCLASS = new PubExtDefaultClass();
AddTestCase( "*** Access default property from private method in sub class ***", 1, 1 );
AddTestCase( "PUBEXTDCLASS.privSubSetDPArray(arr), PUBEXTDCLASS.privSubGetDPArray()", arr, PUBEXTDCLASS.testPrivSubDPArray(arr) );

// <TODO>  fill in the rest of the cases here

// ********************************************
// access default property from
// final method in sub class
// ********************************************

PUBEXTDCLASS = new PubExtDefaultClass();
AddTestCase( "*** Access default property from final method in sub class ***", 1, 1 );
AddTestCase( "PUBEXTDCLASS.finSubSetDPArray(arr), PUBEXTDCLASS.finSubGetDPArray()", arr, PUBEXTDCLASS.testFinSubDPArray(arr) );


// ********************************************
// Class Prototype Testing
// ********************************************

//Add new property to parent through prototype object, verify child inherits it
var child = new PubExtDefaultClass();
DefaultClassInner.prototype.fakeProp = 100;
AddTestCase("*** Add new property to parent prototype object, verify child class inherits it ***", 100, child.fakeProp);

//Try overriding parent property through prototype object, verify child object has correct value
DefaultClassInner.prototype.pObj = 2;
child = new PubExtDefaultClass();
AddTestCase("*** Try overriding parent property through prototype object, verify child object has correct value ***", 1, child.pObj);

test();       // leave this alone.  this executes the test cases and
              // displays results.

}
