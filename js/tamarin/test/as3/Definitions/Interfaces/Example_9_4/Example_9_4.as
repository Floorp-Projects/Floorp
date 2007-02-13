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
// 9.4 example (excluding error cases)
package Example_9_4 {

	interface T {
		function f();
	}
	
	interface U {
		function f();
		function g();
	}
	
	interface V extends T, U {
		function h();
	}
	
	class A implements V {
		public function f() {
			return "a.{T,U}::f()";
		}
		public function g() {
			return "a.U::g()";
		}
		public function h() {
			return "a.V::h()";
		}
	}

	public class ExampleTest {
		var a : A = new A;
		var t : T = a;
		var u : U = a;
		var v : V = a;
		var o = a;
		public function doUniqueTest() : String {
			return t.f() + "," + u.g();
		}
		public function doBlendTest() : String {
			return v.g() + "," + v.h();
		}
		public function doImplementsTest() : String {
			return a.g() + "," + a.U::f();
		}
		public function doUntypedTest() : String {
			return o.T::f();
		}
	}
}

