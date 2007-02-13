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

package GetSetProtectedExtended{

	internal class GetSetProtectedInternal {
	
		private var _nt = "no type";
		private var _x:Array = [1,2,3];
		private var _y:int = -10;
		private var _b:Boolean = true;
		private var _u:uint = 101;
		private var _s:String = "myString";
		private var _n:Number = 555;

		protected function get x():Array {
			return _x;
		}
		protected function set x( a:Array ) {
			_x=a;
		}
		protected function get y():int {
			return _y;
		}
		protected function set y( i:int ) {
			_y=i;
		}
		protected function get boolean():Boolean { return _b; }
		protected function set boolean(b:Boolean):void { _b=b; }
		protected function get u():uint{ return _u; }
		protected function set u(u:uint):void { _u=u; }
		protected function get n():Number{ return _n; }
		protected function set n(num:Number):void { _n=num; }
		protected function get string():String{ return _s; }
		protected function set string(s:String):void { _s=s; }
		protected function get noType(){ return _nt; }
		protected function set noType(nt):void { _nt=nt; }
	
		
		public function getBoolean() {
			return boolean;
		}
		public function setBoolean(b:Boolean) {
			boolean = b;
			return boolean;
		}
		
		public function getArray() {
			return x;
		}
		public function setArray(a:Array) {
			x = a;
			return x;
		}
		public function getUint() {
			return u;
		}
		public function setUint(u:uint) {
			return u;
		}
		public function getString() {
			return string;
		}
		public function setString(s:String) {
			string = s;
			return string;
		}
		public function getNoType() {
			return noType;
		}
		public function setNoType(nt) {
			noType = nt;
			return noType;
		}
		
	}

	public class GetSetProtectedExtended extends GetSetProtectedInternal {
	}
}
