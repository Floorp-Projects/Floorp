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
// ActionScript file
package SuperExprChainAccess {

	class base {
		function f() : String {
			return "base f()";
		}
		function g() : String {
			return "base g()";
		}
		function i() : String {
			return "base i()";
		}
	}
	
	class middle1 extends base {
		override function f() : String {
			return "middle1 f()";
		}
		function h() : String {
			return "middle1 h()";
		}
	}
	
	class middle2 extends middle1 {
		override function h() : String {
			return "middle2 h()";
		}
		override function i() : String {
			return "middle2 i()";
		}
		function callh() : String {
			return super.h();
		}
		function calli() : String {
			return super.i();
		}
	}
	
	class derived extends middle2 {
		override function f() : String {
			return "derived f()";
		}
		override function g() : String {
			return "derived g()";
		}
		
		public function callf1() : String {
			return f();
		}
		public function callf2() : String {
			return super.f();
		}
		public function callg1() : String {
			return g();
		}
		public function callg2() : String {
			return super.g();
		}
		public function callh1() : String {
			return h();
		}
		public function callh2() : String {
			return super.h();
		}
		public function callh3() : String {
			return callh();
		}
		public function calli1() : String {
			return i();
		}
		public function calli2() : String {
			return super.i();
		}
		public function calli3() : String {
			return calli();
		}
	}
	
	public class SuperExprChainAccess extends derived { }
}
