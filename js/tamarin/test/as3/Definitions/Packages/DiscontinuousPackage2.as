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
 
package Discontinuous.Pack {

	public interface IClickable {
		function whoAmI():String; 
	}
}

package Discontinuous.Pack {

	public class A {
		public function whoAmI():String {
			return "A";
		}
	
	}
}

package Discontinuous.Pack {
	use namespace Kitty;
	
	public class C {
		Kitty var b:Boolean = true;
		public function returnNSVar():Boolean {
			return Kitty::b;
		}
		
		Kitty function returnArray():Array {
			return [1,2,3];
		}
		
		public function callNSFunc() {
			var k:Array = Kitty::returnArray();
			return k;
		}
	}
}

package Discontinuous.Pack {

	public class B implements IClickable {
		public function whoAmI():String {
			return "B";
		}
	
	}

}

package Discontinuous.Pack {

	public var s:String = "hello";
	public function publicFunc(i:int):String {
		return "You passed " + i.toString();
	}
	
	public namespace Kitty;
	
}

import Discontinuous.Pack.*;

var SECTION = "Definitions";       // provide a document reference (ie, Actionscript section)
var VERSION = "AS 3.0";        // Version of ECMAScript or ActionScript 
var TITLE   = "PackageDefinition" //Proved ECMA section titile or a description
var BUGNUMBER = "";

startTest();                // leave this alone


var a = new A();
var b = new B();
var c = new C();

AddTestCase("Function defined in [Package.Name]/A.as", "A", a.whoAmI());
AddTestCase("Interface defined in [Package.Name]/B.as", "B", b.whoAmI());
AddTestCase("Variable defined in [Package.Name]/vars.as", "hello", s);
AddTestCase("Function defined in [Package.Name]/vars.as", "You passed 5", publicFunc(5));
AddTestCase("Namespace variable defined in [Package.Name]/C.as", true, c.returnNSVar());
AddTestCase("Namespace function defined in [Package.Name]/C.as", "1,2,3", c.callNSFunc().toString());

class X{
Kitty var num:Number = 5;
Kitty function kittyFunc(s:String):String {
	return "You said hi";
}
}

var obj = new X();
AddTestCase("Variable, namespace defined in [Package.Name]/vars.as", 5, obj.Kitty::num);
AddTestCase("Function, namespace defined in [Package.Name]/vars.as", "You said hi", obj.Kitty::kittyFunc("hi"));

test();       // leave this alone.  this executes the test cases and
              // displays results.
