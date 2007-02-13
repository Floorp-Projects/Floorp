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
// 8 / 9 / 16.3.3 a variety of implements multiple and extends plus implements single / multiple
package ImplementMultipleInterfaces {
	interface A {
		function a();
	}
	interface B {
		function b();
	}
	interface C {
		function c();
	}
	interface D {
		function d();
	}

	class X1 implements A {
		public function a() {
			return "x1.A::a()";
		}
	}
	class X2 implements A, B {
		public function a() {
			return "x2.A::a()";
		}
		public function b() {
			return "x2.B::b()";
		}
	}
	class X3 implements A, B, C {
		public function a() {
			return "x3.A::a()";
		}
		public function b() {
			return "x3.B::b()";
		}
		public function c() {
			return "x3.C::c()";
		}
	}
	class X4 implements A, B, C, D {
		public function a() {
			return "x4.A::a()";
		}
		public function b() {
			return "x4.B::b()";
		}
		public function c() {
			return "x4.C::c()";
		}
		public function d() {
			return "x4.D::d()";
		}
	}
	
	class Y1 extends X1 implements B {
		public function b() {
			return "y1.B::b()";
		}
	}
	class Y2 extends X1 implements B, C {
		public function b() {
			return "y2.B::b()";
		}
		public function c() {
			return "y2.C::c()";
		}
	}
	class Y3 extends X2 implements C, D {
		public function c() {
			return "y3.C::c()";
		}
		public function d() {
			return "y3.D::d()";
		}
	}
	class Y4 extends X3 implements D {
		public function d() {
			return "y4.D::d()";
		}
	}

	public class ImplementTest {
		var x1 : X1 = new X1();
		var x2 : X2 = new X2();
		var x3 : X3 = new X3();
		var x4 : X4 = new X4();
		var y1 : Y1 = new Y1();
		var y2 : Y2 = new Y2();
		var y3 : Y3 = new Y3();
		var y4 : Y4 = new Y4();
		public function doTestX1() : String {
			return x1.a();
		}
		public function doTestX2() : String {
			return x2.a() + "," + x2.b();
		}
		public function doTestX3() : String {
			return x3.a() + "," + x3.b() + "," + x3.c();
		}
		public function doTestX4() : String {
			return x4.a() + "," + x4.b() + "," + x4.c() + "," + x4.d();
		}
		public function doTestY1() : String {
			return y1.a() + "," + y1.b();
		}
		public function doTestY2() : String {
			return y2.a() + "," + y2.b() + "," + y2.c();
		}
		public function doTestY3() : String {
			return y3.a() + "," + y3.b() + "," + y3.c() + "," + y3.d();
		}
		public function doTestY4() : String {
			return y4.a() + "," + y4.b() + "," + y4.c() + "," + y4.d();
		}
	}
}

