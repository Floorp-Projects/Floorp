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
package InternalClassPackage {
	class InternalClass {
		internal var array:Array;
		internal var boolean:Boolean;
		internal var date:Date;
		internal var myFunction:Function;
		internal var math:Math;
		internal var number:Number;
		internal var object:Object;
		internal var string:String;

		// ****************
		// constructor
		// ****************
		function InternalClass() {}

		// *****************
		// public methods
		// *****************
		public function getArray() : Array { return array; }
		public function getBoolean() : Boolean { return boolean; }
		public function getDate() : Date { return date; }
		public function getFunction() : Function { return myFunction; }
		public function getMath() : Math { return math; }
		public function getNumber() : Number { return number; }
		public function getObject() : Object { return object; }
		public function getString() : String { return string; }

		public function setArray( a:Array ) { array = a; }
		public function setBoolean( b:Boolean ) { boolean = b; }
		public function setDate( d:Date ) { date = d; }
		public function setFunction( f:Function ) { myFunction = f; }
		public function setMath( m:Math ) { math = m; }
		public function setNumber( n:Number ) { number = n; }
		public function setObject( o:Object ) { object = o; }
		public function setString( s:String ) { string = s; }

	}

}
