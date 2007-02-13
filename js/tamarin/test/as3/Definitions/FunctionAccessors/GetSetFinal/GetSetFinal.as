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

package GetSetFinal {

	/*public class GetSetFinal extends GetSetFinalSuper {
	  
	  
   	}*/
   	public final class GetSetFinal {
	
		private var _nt = "no type";
		private var _x:Array = [1,2,3];
		private var _y:int = -10;
		private var _b:Boolean = true;
		private var _u:uint = 1;
		private var _s:String = "myString";

		private var _n:Number = 555;

		public function get n():Number {
			return _n;
		}

		public function set n(num):void {
			_n = num;
		}

		public function get x():Array {
			return _x;
		}
		public function set x( a:Array ) {
			_x=a;
		}
		public function get y():int {
			return _y;
		}
		public function set y( i:int ) {
			_y=i;
		}
		public function get boolean():Boolean { return _b; }
		public function set boolean(b:Boolean) { _b=b; }
		public function get u():uint{ return _u; }
		public function set u(ui:uint) { _u=ui; }
		public function get string():String{ return _s; }
		public function set string(s:String) { _s=s; }
		public function get noType():String{ return _s; }
		public function set noType(nt) { _nt=nt; }

		// call setter from setter
		private var _sfs1:Number = 99;
		private var _sfs2:int = 0; 
		public function get sfs1():Number{ return _sfs1; }
		public function get sfs2():int{ return _sfs2; }
		public function set sfs1(n:Number){
			_sfs1 = n;
		}
		public function set sfs2(i:int){
			sfs1 = i;
			_sfs2 = i;
		}

		// call setter from getter
		private var _sfg1:String = "FAILED";
		private var _sfg2:uint = 0; 
		public function get sfg1():String{ return _sfg1; }
		public function get sfg2():uint{
			sfg1 = "PASSED";
			return _sfg2;
		}
		public function set sfg1(s:String){ _sfg1 = s; }
		public function set sfg2(ui:uint){ _sfg2 = ui; }

		// call getter from setter
		private var _gfs1:String = "FAILED";
		private var _gfs2:String = "PASSED"; 
		public function get gfs1():String{
			return _gfs1;
		}
		public function get gfs2():String{
			return _gfs2;
		}
		public function set gfs1(s:String){
			_gfs1=gfs2;
		}
		public function set gfs2(s:String){
			_gfs2=s;
		}

		// call getter from getter
		private var _gfg1:String = "PASSED";
		private var _gfg2:String = "FAILED"; 
		public function get gfg1():String{
			return _gfg1;
		}
		public function get gfg2():String{
			return gfg1;
		}

		// define a getter for a property and call the undefined setter
		private var _nosetter = "FAILED";
		public function get noSetter(){ return _nosetter; }

		// define a setter for a property and call the undefined getter
		private var _nogetter = "FAILED";
		public function set noGetter(s){ _nogetter = s; }
			
	}

}
