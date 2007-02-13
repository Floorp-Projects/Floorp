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


package foo{
	public var a = "This is var a";

	public function func1(){ return "This is func1"; }

	interface IntA{
		function testInt();
	}

	public class ClassA implements IntA {
		
		v1 var ns = "This is namespace v1";
		v2 var ns = "This is namespace v2";
		public function test(){ return "This is test in ClassA"; }
		public function testInt() { return "This is testInt in ClassA"; }
		public function testNS() {return v1::ns; }
		public function testNS2() {return v2::ns; }
	}
	
	
	// Namespaces
	public namespace v1;
	public namespace v2;

}
package foo{
	public var b = "This is var b";

	public function func2(){ return "This is func2"; }

	interface IntB{
		function testInt();
	}

	public class ClassB implements IntB {
		public function test(){ return "This is test in ClassB"; }
		public function testInt() { return "This is testInt in ClassB"; }
	}

	// use the definitions from the first package foo
	public var c = a;
	public function func3(){ return func1(); }

	public var classC = new ClassA();

	public class ClassD implements IntA {
		public function testInt() { return "This is testInt from ClassD"; }
	}

	public class ClassN {
		use namespace v1;
		v1 var ns2 = "This is namespace v1";
		public function getNSVar() {
			return v1::ns2;
		}
	}
}

import foo.*;

var SECTION = "Definitions";       // provide a document reference (ie, Actionscript section)
var VERSION = "AS 3.0";        // Version of ECMAScript or ActionScript 
var TITLE   = "PackageDefinition" //Proved ECMA section titile or a description
var BUGNUMBER = "";

startTest();                // leave this alone


var classA = new ClassA();
var classB = new ClassB();
var classD = new ClassD();
var classN = new ClassN();
 
AddTestCase( "variable a in first definition of foo", "This is var a", a );
AddTestCase( "variable b in second definition of foo", "This is var b", b );
AddTestCase( "function func1 in first definition of foo", "This is func1", func1() );
AddTestCase( "function func2 in second definition of foo", "This is func2", func2() );
AddTestCase( "ClassA in first definition of foo", "This is test in ClassA", classA.test() );
AddTestCase( "ClassB in second definition of foo", "This is test in ClassB", classB.test() );
AddTestCase( "IntA in first definition of foo", "This is testInt in ClassA", classA.testInt() );
AddTestCase( "IntB in second definition of foo", "This is testInt in ClassB", classB.testInt() );
AddTestCase( "namespace v1 in first definition of foo", "This is namespace v1", classA.testNS() );
AddTestCase( "namespace v2 in second definition of foo", "This is namespace v2", classA.testNS2() );

// second foo package uses first foo package
AddTestCase( "access variable a from first package in second", "This is var a", c );
AddTestCase( "access function a from first package in second", "This is func1", func3() );
AddTestCase( "access classA from first package in second", "This is test in ClassA", classC.test() );
AddTestCase( "access IntA from first package in second", "This is testInt from ClassD", classD.testInt() );
AddTestCase( "access namespace from first package in second", "This is namespace v1", classN.getNSVar() );

test();       // leave this alone.  this executes the test cases and
              // displays results.
