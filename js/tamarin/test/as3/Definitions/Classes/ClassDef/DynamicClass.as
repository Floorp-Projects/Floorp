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
 
import DynamicClassSame.*;

var SECTION = "Definitions";           // provide a document reference (ie, ECMA section)
var VERSION = "AS3";                   // Version of JavaScript or ECMA
var TITLE   = "Access Class Properties & Methods";  // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone



var Obj = new DynamicClassAccessor();

// Properties

// Access default property from same package
AddTestCase( "Access default property from same package", "1,2,3", Obj.accDefProp().toString() );

// Access internal property from same package
AddTestCase( "Access internal property from same package", 100, Obj.accIntProp() );

// Access protected property from same package - NOT LEGAL OUTSIDE DERIVED CLASS
//AddTestCase( "Access protected property from same package", -1, Obj.accProtProp() );

// Access public property from same package
AddTestCase( "Access public property from same package", 1, Obj.accPubProp() );

// Access private property from same class and package
AddTestCase( "Access private property from same class and package", true, Obj.accPrivProp() );

// Access namespace property from same package
AddTestCase( "Access namespace property from same package", "nsProp", Obj.accNSProp() );

// Access static public property from same package
AddTestCase( "Access public static property from same package", true, Obj.accPubStatProp() );


// Methods


// Access default method from same package
AddTestCase( "Access default method from same package", true, Obj.accDefMethod() );

// Access internal method from same package
AddTestCase( "Access internal method from same package", 50, Obj.accIntMethod() );

// Access protected method from same package - NOT LEGAL OUTSIDE DERIVED CLASS
//AddTestCase( "Access protected method from same package", 1, Obj.accProtMethod() );

// Access public method from same package
AddTestCase( "Access public method from same package", true, Obj.accPubMethod() );

// Access private method from same class and package
AddTestCase( "Access private method from same class and package", true, Obj.accPrivMethod() );

// Access namespace method from same package
AddTestCase( "Access namespace method from same package", "nsMethod", Obj.accNSMethod() );

// Access final public method from same package
AddTestCase( "Access final public method from same package", 1, Obj.accPubFinMethod() );

// Access static public method from same package
AddTestCase( "Access static public method from same package", 42, Obj.accPubStatMethod() );

// Error cases

// access private property from same package not same class
var thisError = "no error thrown";
try{
	Obj.accPrivPropErr();
} catch (e) {
	thisError = e.toString();
} finally {
	AddTestCase( "Access private property from same package not same class", "ReferenceError: Error #1065",
				referenceError( thisError ) );
}

// access private method from same package not same class
thisError = "no error thrown";
try{
	Obj.accPrivMethErr();
} catch (e2) {
	thisError = e2.toString();
} finally {
	AddTestCase( "Access private method from same package not same class", "ReferenceError: Error #1065",
				referenceError( thisError ) );
}


test();       // leave this alone.  this executes the test cases and
              // displays results.
