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
 

 package A {
	public class A {
		public function whoAmI():String {
			return "A.A";
		}
                
	}
	
	
	public class B  {
		var sB;
		public function createB() {
			sB = new A();
		}
		public function createB2() {
			sB = new A.A();
                   
		}
		public function whoAmI():String {
			return "A.B";
		}
	}
}

package D {
	public var D:String = "hello";
}
package E {
	public var E:String = "E";
        
}
package F {
	public function F(n:Number):String {
		return "You passed " + n;
	}
}
package IG {
	interface IG {
		function whoAmI():String;
	}
	public class G implements IG {
		public function whoAmI():String {
			return "G";
		}
	}
}

import A.*;
import D.*;
import E.*;
import F.*;
import IG.*;

var SECTION = "Definitions";       // provide a document reference (ie, Actionscript section)
var VERSION = "AS 3.0";        // Version of ECMAScript or ActionScript 
var TITLE   = "PackageDefinition" //Proved ECMA section titile or a description
var BUGNUMBER = "";

startTest();                // leave this alone


var a = new A();
var b = new B();
var g = new G();

AddTestCase("Class A in package A", "A.A", a.whoAmI());
AddTestCase("Class B in package A", "A.B", b.whoAmI());

result = "";
try {
	b.createB();
	result = "no exception";
} catch (e2) {
	result = "exception";
}

// !!! This will have to be changed when bug 139002 is fixed.

AddTestCase("Access A in A as new A()", "no exception", result);

result = "";
try {
	b.createB2();
	result = "no exception";
} catch (e3) {
	result = "exception";
}

// !!! This will have to be changed when bug 138845 is fixed.
//bug 138845 is fixed so changing "exception" to "no exception"

AddTestCase("Access A in A as new A.A()", "no exception", result);
	
AddTestCase("Variable D in package D", "hello", D);

try {
	e=E.E;
	result = "no exception";
} catch(e) {
	result = "exception";
}
AddTestCase("Public variable E inside package E", "no exception", result);
AddTestCase("Public variable E inside package E", "E", E.E);
AddTestCase("Function F inside package F", "You passed 5", F(5));

AddTestCase("Interface IG defined in package IG", "G", g.whoAmI());

test();       // leave this alone.  this executes the test cases and
              // displays results.
