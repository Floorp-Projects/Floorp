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
// Bug 162570: http://flashqa.macromedia.com/bugapp/detail.asp?ID=162570

package same {
	public class same {
		function same() {}
	}
}

package test {
	public class More {
		function More() {}

		public static var a:int = 0;
		public static function foo(foo:int):void { More.a = foo; }

		public var b:Boolean = false;
		public function bar(bar:Boolean):void { b = bar; }
	}
}


//--------------------------------------------------
startTest();
//--------------------------------------------------

import same.*;
import test.*;

// same package and class name
var s:same = new same();
AddTestCase("s is same", true, (s is same));

// static method and parameter with the same name
AddTestCase("More.foo(1)", 1, (More.foo(1), More.a));

// instance method and parameter with the same name
var m:More = new More();
AddTestCase("m.bar(true)", true, (m.bar(true), m.b));

// instance class and method with the same name
var bar:More = new More();
AddTestCase("bar.bar(true)", true, (bar.bar(true), bar.b));

// instance class and property with the same name
var b:More = new More();
AddTestCase("b.bar(true)", true, (b.bar(true), b.b));

// dynamic method and parameter with the same name
dynamic class C {}
var c:C = new C();
c.a = false;
c.b = function (b:Boolean):void { c.a = b; }
AddTestCase("c.b(true)", true, (c.b(true), c.a));

//--------------------------------------------------
test();
