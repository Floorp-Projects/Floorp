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
// 9 / 16.4 instances of classes that implement an interface belong
// to the type represented by the interface
package InterfaceAsType {
	interface A {
		function a();
	}
	interface B {
		function b();
	}
	interface C extends A, B {
	}

	class X implements A, B {
		public function a() {
			return "x.A::a()";
		}
		public function b() {
			return "x.B::b()";
		}
	}
	class Y implements C {
		public function a() {
			return "y.A::a()";
		}
		public function b() {
			return "y.B::b()";
		}
	}

	public class TypeTest {
		var ax : A = new X();
		var bx : B = new X();
		var x : X = new X();
		var ay : A = new Y();
		var by : B = new Y();
		var c : C = new Y();
		var y : Y = new Y();
		public function doCallXViaA() : String {
			return ax.a();
		}
		public function doCallXViaB() : String {
			return bx.b();
		}
		private function callArgA(aa:A) : String {
			return aa.a();
		}
		private function callArgB(bb:B) : String {
			return bb.b();
		}
		public function doCallXViaArgs() : String {
			return callArgA(x) + "," + callArgB(x);
		}
		private function returnXAsA() : A {
			return new X();
		}
		private function returnXAsB() : B {
			return new X();
		}
		public function doCallXViaReturn() : String {
			return returnXAsA().a() + "," + returnXAsB().b();
		}
		
		public function doCallYViaA() : String {
			return ay.a();
		}
		public function doCallYViaB() : String {
			return by.b();
		}
		public function doCallYViaC() : String {
			return c.a() + "," + c.b();
		}
		private function callArgC(cc:C) : String {
			return cc.a() + "," + cc.b();
		}
		public function doCallYViaArgs() : String {
			return callArgA(y) + "," + callArgB(y);
		}
		public function doCallYViaArgC() : String {
			return callArgC(y);
		}
		private function returnYAsA() : A {
			return new Y();
		}
		private function returnYAsB() : B {
			return new Y();
		}
		private function returnYAsC() : C {
			return new Y();
		}
		public function doCallYViaReturn() : String {
			return returnYAsA().a() + "," + returnYAsB().b();
		}
		public function doCallYViaReturnC() : String {
			return returnYAsC().a() + "," + returnYAsC().b();
		}
	}
}

