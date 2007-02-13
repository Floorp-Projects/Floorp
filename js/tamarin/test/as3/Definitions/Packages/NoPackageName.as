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
 
package {
 	public var a = "boo";
 	//private var b = "hoo"; Changing private variable to default variable since
        var b = "hoo";           //private attribute is allowed only on class property
 	
 	public class PublicClass {
 		public function sayHi() {
 			return "hi";
 		}
 		function sayBye() {
 			return "bye";
 		}
 	}
 	
 	/*private class PrivateClass {       //Commenting out since private attribute is                                              //allowed only on class property
 		function sayHi() {
 			return "private hi";
 		}
 	}*/
        
 	
 	class NotAPublicClass {
 		public function sayHi() {
 			return "hi";
 		}
 	}
}

var SECTION = "Definitions";       // provide a document reference (ie, Actionscript section)
var VERSION = "AS 3.0";        // Version of ECMAScript or ActionScript 
var TITLE   = "PackageDefinition" //Proved ECMA section titile or a description
var BUGNUMBER = "";

startTest();                // leave this alone


var helloWorld = "Hello World";
AddTestCase( "var helloWorld = 'Hello World'", "Hello World", helloWorld );

AddTestCase("Access public variable in package with no name", "boo", a);

var expected = 'error';
var actual = 'no error';

try {
	var internalVar = b;
} catch(e1) {
	actual = 'error';
}

AddTestCase("Access internal variable in package with no name", expected, actual);


AddTestCase("Access public class in package with no name", "hi", (c1 = new PublicClass(), c1.sayHi()));

var expected = 'error';
var actual = 'no error';

try {
	var c2 = new PublicClass();
	c2.sayBye();
} catch(e2) {
	actual = 'error';
}

AddTestCase("Try to access function not declared as public", expected, actual);

var expected = 'error';
var actual = 'no error';

/*try {
	var c3 = new PrivateClass();
	c3.sayHi();
} catch(e3) {
	actual = 'error';
}

AddTestCase("Try to access private variable", expected, actual);*/


var expected = 'error';
var actual = 'no error';

try {
	var c4 = new NotAPublicClass();
	c4.sayHi();
} catch(e4) {
	actual = 'error';
}

AddTestCase("Try to access class not declared as public", expected, actual);

test();       // leave this alone.  this executes the test cases and
              // displays results.
