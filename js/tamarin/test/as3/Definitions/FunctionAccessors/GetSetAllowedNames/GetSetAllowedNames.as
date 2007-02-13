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
package GetSetAllowedNames{
 public class GetSetAllowedNames {
 
 	
 	var __a1 = "_a1";
 	var ___a1 = "__a1";
 	var ___a1__ = "__a1__";
 	var __a1_ = "_a1_";
 	var _$a1 = "$a1";
 	var _a$1 = "a$1";
 	var _a1$ = "a1$";
 	var _A1 = "A1";
 	var _cases = "cases";
 	var _Cases = "Cases";
 	var _all = "all";
 	var _get = "get";
 	var _set = "set";
 	
 	private var _Function = function() { }
 	private var _getSetAllowedNames = "constructor, different case";
 	private var _Class = "class";

    	
	public function get _a1 () { return __a1; }
	public function get _a1_ () { return __a1_; }
	public function get __a1__ () { return ___a1__; }
	public function get $a1 () { return _$a1; }
	public function get a$1 () { return _a$1; }
	public function get a1$ () { return _a1$; }
	public function get A1 () { return _A1; }
	public function get cases () { return _cases; }
	public function get Cases () { return _Cases; }
	public function get abcdefghijklmnopqrstuvwxyz0123456789$_ () { return _all; }
	
	
	public function set _a1 (a) { __a1= a;}
	public function set _a1_ (a) { __a1_= a;}
	public function set __a1__ (a) { ___a1__= a;}
	public function set $a1 (a) { _$a1= a;}
	public function set a$1 (a) { _a$1= a;}
	public function set a1$ (a) { _a1$= a;}
	public function set A1 (a) { _A1= a;}
	public function set cases (a) { _cases= a;}
	public function set Cases (a) { _Cases= a;}
	public function set abcdefghijklmnopqrstuvwxyz0123456789$_ (a) { _all= a;}
	
	public function get get() { return _get; }
	public function set get(e) { _get = e; }
	public function get set() { return _set; }
	public function set set(e) { _set = e;}
	
	public function get FuncTion():Function { return _Function; }
	public function set FuncTion(f:Function):void { _Function = f; }
	
	public function get getSetAllowedNames():String { return _getSetAllowedNames; }
	public function set getSetAllowedNames(s:String):void { _getSetAllowedNames = s; }
	
	public function get Class():String { return _Class; }
	public function set Class(c:String):void { _Class = c; }

   }
}

