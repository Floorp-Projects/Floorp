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
// 9 get / set allowed in interfaces
package GetSet {
	interface A {
		function get a() : String;
		function set a(aa:String) : void;
		function get b() : String;
		function set c(cc:String) : void;
	}
	interface B {
		function set b(bb:String) : void;
		function get c() : String;
	}

	class X implements A {
		var a_ : String = "unset";
		var c_ : String = "unset";
		public function get a() : String {
			return "x.A::get a()";
		}
		public function set a(x:String) : void {
			a_ = "x.A::set a()";
		}
		public function getA() : String {
			return a_;
		}
		public function get b() : String {
			return "x.A::get b()";
		}
		public function set c(x:String) : void {
			c_ = "x.A::set c()";
		}
		public function getC() : String {
			return c_;
		}
	}
	class Y implements A, B {
		var a_ : String = "unset";
		var b_ : String = "unset";
		var c_ : String = "unset";
		public function get a() : String {
			return "y.A::get a()";
		}
		public function set a(x:String) : void {
			a_ = x;
		}
		public function getA() : String {
			return a_;
		}
		public function get b() : String {
			return "y.A::get b()";
		}
		public function set b(x:String) : void {
			b_ = x;
		}
		public function getB() : String {
			return b_;
		}
		public function get c() : String {
			return "y.B::get c()";
		}
		public function set c(x:String) : void {
			c_ = x;
		}
		public function getC() : String {
			return c_;
		}
	}

	public class GetSetTest {
		var x : X = new X();
		var y : Y = new Y();
		public function doGetAX() : String {
			return x.a;
		}
		public function doSetAX() : String {
			x.a = "ignored";
			return x.getA();
		}
		public function doGetBX() : String {
			return x.b;
		}
		public function doSetCX() : String {
			x.c = "ignored";
			return x.getC();
		}
		public function doGetAY() : String {
			return y.a;
		}
		public function doSetAY() : String {
			y.a = "y.A::set a()";
			return y.getA();
		}
		public function doGetBY() : String {
			return y.b;
		}
		public function doSetBY() : String {
			y.b = "y.B::set b()";
			return y.getB();
		}
		public function doGetCY() : String {
			return y.c;
		}
		public function doSetCY() : String {
			y.c = "y.A::set c()";
			return y.getC();
		}
	}
}

