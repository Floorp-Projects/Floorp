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
package PublicClassPackage {
	public class PublicClass {

		var array:Array;							// Default property
		var finNumber:Number;					// Default final property
		static var statFunction:Function;			// Default Static property
		static var finStatNumber:Number;		// Default final static property
		// TODO: virtual vars are not implemented yet
		// virtual var virtArray:Array;
		
		internal var internalArray:Array;							// Internal property
		internal var internalFinNumber:Number;				// Internal final property
		internal static var internalStatFunction:Function;			// Internal static property
		internal static var internalFinStatNumber:Number;		// Internal Final Static property
		// TODO: virtual vars are not implemented yet
		// internal virtual var internalVirtNumber:Number;			// internal virtual property

		private var privDate:Date;								// Private property
		private static var privStatString:String;				// Private Static property
		private var privFinalString:String;				// Private Final property
		private static var privFinalStaticString:String	// Private Final Static property
		// TODO: virtual vars are not implemented yet
		// private virtual privVirtDate:Date;

		public var pubBoolean:Boolean;						// Public property
		public static var pubStatObject:Object;				// Public Static property
		public var pubFinArray:Array;					// Public Final property
		public static var pubFinalStaticNumber:Number	// Public Final Static property
		// TODO: virtual vars are not implemented yet
		// public virtual var pubVirtBoolean:Boolean;


		// Testing property for constructor testing
		public static var constructorCount : int = 0;
		
		// *****************
		// Constructors
		// *****************
		public function PublicClass () {
			PublicClass.constructorCount ++;
		}
		

		// *****************
		// Default methods
		// *****************
		function getArray() : Array { return array; }
		function setArray( a:Array ) { array = a; }
		// wrapper function
		public function testGetSetArray(a:Array) : Array {
			setArray(a);
			return getArray();
		}
		
		
		// ************************
		// Default virtual methods
		// ************************
		// TODO: virtual vars are not implemented yet so this is currently using a normal var
		virtual function getVirtualArray() : Array { return array; }
		virtual function setVirtualArray( a:Array ) { array = a; }
		// wrapper function
		public function testGetSetVirtualArray(a:Array) : Array {
			setVirtualArray(a);
			return getVirtualArray();
		}
		
		
		// ***********************
		// Default Static methods
		// ***********************
		static function setStatFunction(f:Function) { PublicClass.statFunction = f; }
		static function getStatFunction() : Function { return PublicClass.statFunction; }
		// wrapper function
		public function testGetSetStatFunction(f:Function) : Function {
			PublicClass.setStatFunction(f);
			return PublicClass.getStatFunction();
		}
        
        
        // **********************
        // Default Final methods
		// **********************
		final function setFinNumber(n:Number) { finNumber = n; }
		final function getFinNumber() { return finNumber; }
		// wrapper function
		public function testGetSetFinNumber(n:Number) : Number {
			setFinNumber(n);
			return getFinNumber();
		}

		
		
		// *****************
		// Internal methods
		// *****************
		internal function getInternalArray() : Array { return internalArray; }
		internal function setInternalArray( a:Array ) { internalArray = a; }
		// wrapper function
		public function testGetSetInternalArray(a:Array) : Array {
			setInternalArray(a);
			return getInternalArray();
		}
		
		
		// *************************
		// Internal virtual methods
		// ************************
		// TODO: virtual vars are not implemented yet so this is currently using a normal var
		internal virtual function getInternalVirtualArray() : Array { return internalArray; }
		internal virtual function setInternalVirtualArray( a:Array ) { internalArray = a; }
		// wrapper function
		public function testGetSetInternalVirtualArray(a:Array) : Array {
			setInternalVirtualArray(a);
			return getInternalVirtualArray();
		}
		
		
		// ***********************
		// Internal Static methods
		// ***********************
		internal static function setInternalStatFunction(f:Function) { internalStatFunction = f; }
		internal static function getInternalStatFunction() { return internalStatFunction; }
		// wrapper function
		public function testGetSetInternalStatFunction(f:Function) : Function {
			setInternalStatFunction(f);
			return getInternalStatFunction();
		}
        
        
        
        // **********************
        // Internal Final methods
		// **********************
		internal final function setInternalFinNumber(n:Number) { internalFinNumber = n; }
		internal final function getInternalFinNumber() { return internalFinNumber; }
		// wrapper function
		public function testGetSetInternalFinNumber(n:Number) : Number {
			setInternalFinNumber(n);
			return getInternalFinNumber();
		}

        
        
		// *******************
		// Private methods
		// *******************
		private function getPrivDate() : Date { return privDate; }
		private function setPrivDate( d:Date ) { privDate = d; }
		// wrapper function
		public function testGetSetPrivDate(d:Date) : Date {
			setPrivDate(d);
			return getPrivDate();
		}

		
		// ************************
		// Private virtual methods
		// ***********************
		// TODO: virtual vars are not implemented yet so this is currently using a normal var
		private virtual function getPrivVirtualDate() : Date { return privDate; }
		private virtual function setPrivVirtualDate( d:Date ) { privDate = d; }
		// wrapper function
		public function testGetSetPrivVirtualDate(d:Date) : Date {
			setPrivVirtualDate(d);
			return getPrivVirtualDate();
		}
		

		// **************************
		// Private Static methods
		// **************************
		private static function setPrivStatString(s:String) { PublicClass.privStatString = s; }
		private static function getPrivStatString() { return PublicClass.privStatString; }
		// wrapper function
		public function testGetSetPrivStatString(s:String) : String {
			PublicClass.setPrivStatString(s);
			return PublicClass.getPrivStatString();
		}
		
		
		// **************************
		// Private Final methods
		// **************************
		private final function setPrivFinalString(s:String) { privFinalString = s; }
		private final function getPrivFinalString() { return privFinalString; }
		// wrapper function
		public function testGetSetPrivFinalString(s:String) : String {
			setPrivFinalString(s);
			return getPrivFinalString();
		}

		
		
		// *******************
		// Public methods
		// *******************
		public function setPubBoolean( b:Boolean ) { pubBoolean = b; }
		public function getPubBoolean() : Boolean { return pubBoolean; }
		
		
		// ************************
		// Public virutal methods
		// ***********************
		// TODO: virtual vars are not implemented yet so this is currently using a normal var
		public virtual function setPubVirtualBoolean( b:Boolean ) { pubBoolean = b; }
		public virtual function getPubVirtualBoolean() : Boolean { return pubBoolean; }


		// **************************
		// Public Static methods
		// **************************
		public static function setPubStatObject(o:Object) { PublicClass.pubStatObject = o; }
		public static function getPubStatObject() { return PublicClass.pubStatObject; }


		// *******************
		// Public Final methods
		// *******************
		public final function setPubFinArray(a:Array) { pubFinArray = a; }
		public final function getPubFinArray() { return pubFinArray; }

	}
}
