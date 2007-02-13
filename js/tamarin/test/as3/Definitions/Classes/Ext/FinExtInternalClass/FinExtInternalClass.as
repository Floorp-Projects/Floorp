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
 * Portions created by the Initial Developer are Copyright (C) 2004-2006
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


// This statement should be inside the package definition. NOT HERE.
// import Definitions.Classes.InternalClass;

// The package definition has been changed, and now all the packages will be under the 
// folder Definitions.Classes, and only this will be used and not the subfolders within.
// This means that we cannot import a single Class file as was required to be done earlier.
// All the files in the package folder will be compiled and used.

// IMPORTANT: This might change later, hence only commenting out the import lines 
//            and not deleting them. This also creates a problem that the compiled file
//            might become too big to handle.
package InternalClass {

        import InternalClass.*;

	// Commenting out the import statement as this is not allowed by the compiler now
	// import Definitions.Classes.InternalClass;

	// As this is a final class, it will not be seen by the testcase, hence
	// creating a wrapper function for this and changing the name for this file.
	// FinExtInternalClass extends FinExtInternalClassInner

	final class FinExtInternalClass extends InternalClass {

		// access default method of parent from default method of sub class
		function subSetArray( a:Array )  { setArray( a ); }
		function subGetArray() : Array { return getArray(); }


		// access default method of parent from dynamic method of sub class
		// mt: dynamic methods are not supported
		function dynSubSetArray( a:Array )  { setArray( a ); }		
		function dynSubGetArray() : Array { return getArray(); }
		

		// access default method of parent from public method of sub class
		public function pubSubSetArray( a:Array ) { setArray( a ); }
		public function pubSubGetArray() : Array { return getArray(); }


		// access default method of parent from static method of sub class
		// This is an error case as a static method cannot access any other 
		// method except a static method and will be addressed in a seperate 
		// file in the Error folder.
		// static function statSubSetArray( a:Array ) { setArray( a ); }
		// static function statSubGetArray() : Array { return getArray(); }


		// access default method of parent from final method of sub class
		final function finSubSetArray( a:Array )  { setArray( a ); }
		final function finSubGetArray() : Array { return getArray(); }


		// access default method of parent from virtual method of sub class
		virtual function virSubSetArray( a:Array ) { setArray( a ); }
		virtual function virSubGetArray() : Array { return getArray(); }


		// access default method of parent from private method of sub class
		private function privSubSetArray( a:Array ) { setArray( a ); }
		private function privSubGetArray() : Array { return getArray(); }

		// function to test above using a public function within the class
		// this function will be accessible to an object of the class.
		public function testPrivSubArray( a:Array ) : Array {
			this.privSubSetArray( a );
			return this.privSubGetArray();
		}



		// access default method of parent from public dynamic method of sub class
		// dynamic methods are not supported
		public function pubDynSubSetArray( a:Array ) { setArray( a ); }
		public function pubDynSubGetArray() : Array { return getArray(); }

		
		// access default method of parent from public static method of sub class
		// This is an error case as a static method cannot access any other 
		// method except a static method and will be addressed in a seperate 
		// file in the Error folder.
		// public static function pubStatSubSetArray( a:Array )  { setArray( a ); }
		// public static function pubStatSubGetArray() : Array { return getArray(); }


		// access default method of parent from public final method of sub class
		public final function pubFinSubSetArray( a:Array )  { setArray( a ); }		
		final public function pubFinSubGetArray() : Array { return getArray(); }
		

		// access default method of parent from public virtual method of sub class
		public virtual function pubVirSubSetArray( a:Array ) { setArray( a ); }
		virtual public function pubVirSubGetArray() : Array { return getArray(); }


		// access default method of parent from dynamic static method of sub class
		// This is an error case as a static method cannot access any other 
		// method except a static method and will be addressed in a seperate 
		// file in the Error folder.	
		// dynamic static function dynStatSubSetArray( a:Array )  { setArray( a ); }
		// dynamic static function dynStatSubGetArray() : Array { return getArray(); }


		// access default method of parent from dynamic final method of sub class
		final function dynFinSubSetArray( a:Array )  { setArray( a ); }		
		final function dynFinSubGetArray() : Array { return getArray(); }
		

		// access default method of parent from dynamic virtual method of sub class
		virtual function dynVirSubSetArray( a:Array ) { setArray( a ); }
		virtual function dynVirSubGetArray() : Array { return getArray(); }


		// access default method of parent from dynamic private method of sub class
		private function dynPrivSubSetArray( a:Array ) { setArray( a ); }
		private function dynPrivSubGetArray() : Array { return getArray(); }

		// function to test above from test scripts
		public function testPrivDynSubArray( a:Array ) : Array {
			this.dynPrivSubSetArray( a );
			return this.dynPrivSubGetArray();
		}


		// access default method of parent from final static method of sub class
		// This is an error case as a static method cannot access any other 
		// method except a static method and will be addressed in a seperate 
		// file in the Error folder.
		// final static function finStatSubSetArray( a:Array )  { setArray( a ); }
		// static final function finStatSubGetArray() : Array { return getArray(); }


		// access default method of parent from final virtual method of sub class
		// This is an invalid case and compiler should report that a function cannot 
		// be both final and virtual.
		// This case will be moved to the negative test cases - Error folder.
		// final virtual function finVirSubSetArray( a:Array ) { setArray( a ); }
		// virtual final function finVirSubGetArray() : Array { return getArray(); }


		// access default method of parent from final private method of sub class
		final private function finPrivSubSetArray( a:Array ) { setArray( a ); }
		private final function finPrivSubGetArray() : Array { return getArray(); }

		// function to test above from test scripts
		public function testPrivFinSubArray( a:Array ) : Array {
			this.finPrivSubSetArray( a );
			return this.finPrivSubGetArray();
		}


		// access default method of parent from private static method of sub class
		// This is an error case as a static method cannot access any other 
		// method except a static method and will be addressed in a seperate 
		// file in the Error folder.		
		private static function privStatSubSetArray( a:Array ) { setArray( a ); }
		static private function privStatSubGetArray() : Array { return getArray(); }

		// function to test above from test scripts
		public function testPrivStatSubArray( a:Array ) : Array {
			this.privStatSubSetArray( a );
			return this.privStatSubGetArray();
		}


		// access default method of parent from private virtual method of sub class
		private virtual function privVirSubSetArray( a:Array ) { setArray( a ); }
		virtual private function privVirSubGetArray() : Array { return getArray(); }

		// function to test above from test scripts
		public function testPrivVirSubArray( a:Array ) : Array {
			this.privVirSubSetArray( a );
			return this.privVirSubGetArray();
		}

		// access default method of parent from virtual static method of sub class
		// This is an invalid case as a function cannot be both virtual and static.
		// This test case will be moved to the Error folder as a negative test case.
		// virtual static function virStatSubSetArray( a:Array ) { setArray( a ); }
		// static virtual function virStatSubGetArray() : Array { return getArray(); }



		// access default property from default method of sub class
		function subSetDPArray( a:Array ) { array = a; }
		function subGetDPArray() : Array { return array; }


		// access default property from dynamic method of sub class.
		function dynSubSetDPArray( a:Array ) { array = a; }
		function dynSubGetDPArray() : Array { return array; }


		// access default property from public method of sub class
		public function pubSubSetDPArray( a:Array ) { array = a; }
		public function pubSubGetDPArray() : Array { return array; }


		// access default property from private method of sub class
		private function privSubSetDPArray( a:Array ) { array = a; }
		private function privSubGetDPArray() : Array { return array; }

		// public accessor for the above given private functions,
		// so that these can be accessed from an object of the class 
		public function testPrivSubDPArray( a:Array ) : Array {
			privSubSetDPArray( a );
			return privSubGetDPArray();
		}


		// access default property from static method of sub class
		static function statSubSetDPArray( a:Array ) { array = a; }
		static function statSubGetDPArray() : Array { return array; }


		// access default property from final method of sub class
		final function finSubSetDPArray( a:Array ) { array = a; }
		final function finSubGetDPArray() : Array { return array; }


		// access default property from virtual method of sub class
		virtual function virSubSetDPArray( a:Array ) { array = a; }
		virtual function virSubGetDPArray() : Array { return array; }


		// access default property from public static method of sub class
		public static function pubStatSubSetDPArray( a:Array ) { array = a; }
		public static function pubStatSubGetDPArray() : Array { return array; }


		// access default property from private static method of sub class
		private static function privStatSubSetDPArray( a:Array ) { array = a; }
		private static function privStatSubGetDPArray() : Array { return array; }

		// public accessor for the above given private functions,
		// so that these can be accessed from an object of the class 
		public function testPrivStatSubDPArray( a:Array ) : Array {
			privStatSubSetDPArray( a );
			return privStatSubGetDPArray();
		}
	    
	}


	// Create an instance of the final class in the package.
	// This object will be used for accessing the methods created 
	// within the sub class given above.
	var EXTDCLASS = new FinExtInternalClass(); 

	// Create a series of public functions that call the methods.
	// The same names can be used as used in the class; because we are now 
	// outside of the class definition.
	
	// The default method of the sub class.
	public function subSetArray( a:Array )  { EXTDCLASS.subSetArray( a ); }
	public function subGetArray() : Array  { return EXTDCLASS.subGetArray(); }


	// The dynamic method of the sub class.
	public function dynSubSetArray( a:Array )  { EXTDCLASS.dynSubSetArray( a ); }
	public function dynSubGetArray() : Array  { return EXTDCLASS.dynSubGetArray(); }


	// The public method of the sub class.	
	public function pubSubSetArray( a:Array )  { EXTDCLASS.pubSubSetArray( a ); }
	public function pubSubGetArray() : Array  { return EXTDCLASS.pubSubGetArray(); }


	// The private method of the sub class. Only one is used as we need to call only the 
	// test function, which in turn calls the actual private methods, as within the class
	// we can access the private methods; but not outside of the class.
	public function testPrivSubArray( a:Array ) : Array  { return EXTDCLASS.testPrivSubArray( a ); }


	// The static method of the sub class.
	// This case is a negative case and will be put in the Error folder.
	// public function statSubSetArray( a:Array )  { FinClassExtInternalClass.statSubSetArray( a ); }
	// public function statSubGetArray() : Array  { return FinClassExtInternalClass.statSubGetArray(); }

	// The STATIC class cannot be accessed by an object of the class, but has to be accessed
	// by the class name itself. Bug filed for this: 108206
	// public function statSubSetArray( a:Array )  { EXTDCLASS.statSubSetArray( a ); }
	// public function statSubGetArray() : Array  { return EXTDCLASS.statSubGetArray(); }


	// The final method of the sub class.
	public function finSubSetArray( a : Array )  { EXTDCLASS.finSubSetArray( a ); }
	public function finSubGetArray() : Array  { return EXTDCLASS.finSubGetArray(); }
	

	// The virtual method of the sub class.
	public function virSubSetArray( a : Array )  { EXTDCLASS.virSubSetArray( a ); }
	public function virSubGetArray() : Array  { return EXTDCLASS.virSubGetArray(); }

		

	// The public dynamic method of the sub class.
	public function pubDynSubSetArray( a:Array )  { EXTDCLASS.dynSubSetArray( a ); }
	public function pubDynSubGetArray() : Array  { return EXTDCLASS.pubDynSubGetArray(); }


	// The public final method of the sub class.
	public function pubFinSubSetArray( a : Array )  { EXTDCLASS.pubFinSubSetArray( a ); }
	public function pubFinSubGetArray() : Array  { return EXTDCLASS.pubFinSubGetArray(); }
	

	// The public virtual method of the sub class.
	public function pubVirSubSetArray( a : Array )  { EXTDCLASS.pubVirSubSetArray( a ); }
	public function pubVirSubGetArray() : Array  { return EXTDCLASS.pubVirSubGetArray(); }



	// The dynamic private method of the sub class. Only one is used as we need to call only
	// the test function, which in turn calls the actual private methods, as within the class
	// we can access the private methods; but not outside of the class.
	public function testPrivDynSubArray( a:Array ) : Array  { return EXTDCLASS.testPrivDynSubArray( a ); }


	// The dynamic final method of the sub class.
	public function dynFinSubSetArray( a : Array )  { EXTDCLASS.dynFinSubSetArray( a ); }
	public function dynFinSubGetArray() : Array  { return EXTDCLASS.dynFinSubGetArray(); }
	

	// The dynamic virtual method of the sub class.
	public function dynVirSubSetArray( a : Array )  { EXTDCLASS.dynVirSubSetArray( a ); }
	public function dynVirSubGetArray() : Array  { return EXTDCLASS.dynVirSubGetArray(); }	



	// The final private method of the sub class. Only one is used as we need to call only
	// the test function, which in turn calls the actual private methods, as within the class
	// we can access the private methods; but not outside of the class.
	public function testPrivFinSubArray( a:Array ) : Array  { return EXTDCLASS.testPrivFinSubArray( a ); }



	// The private static method of the sub class. Only one is used as we need to call only
	// the test function, which in turn calls the actual private methods, as within the class
	// we can access the private methods; but not outside of the class.
	// This case is a negative case and will be put in the Error folder.	
	public function testPrivStatSubArray( a:Array ) : Array  { return EXTDCLASS.testPrivStatSubArray( a ); }



	// The private virtual method of the sub class. Only one is used as we need to call only
	// the test function, which in turn calls the actual private methods, as within the class
	// we can access the private methods; but not outside of the class.
	public function testPrivVirSubArray( a:Array ) : Array  { return EXTDCLASS.testPrivVirSubArray( a ); }


	// The default property being accessed by the different method attributes.
	// The default method attribute.
	public function subSetDPArray( a:Array ) { EXTDCLASS.subSetDPArray(a); }
	public function subGetDPArray() : Array { return EXTDCLASS.subGetDPArray(); }


	// The dynamic method attribute.
	public function dynSubSetDPArray( a:Array ) { EXTDCLASS.dynSubSetDPArray(a); }
	public function dynSubGetDPArray() : Array { return EXTDCLASS.dynSubGetDPArray(); }


	// The public method attribute.
	public function pubSubSetDPArray( a:Array ) { EXTDCLASS.pubSubSetDPArray(a); }
	public function pubSubGetDPArray() : Array { return EXTDCLASS.pubSubGetDPArray(); }


	// The private method attribute.
	public function testPrivSubDPArray( a:Array ):Array { return EXTDCLASS.testPrivSubDPArray(a) }

	// the static method attribute.
	public function statSubSetDPArray( a:Array ) { FinExtInternalClass.statSubSetDPArray(a); }
	public function statSubGetDPArray() : Array { return FinExtInternalClass.statSubGetDPArray(); }


	// the final method attribute.
	public function finSubSetDPArray( a:Array ) { EXTDCLASS.finSubSetDPArray(a); }
	public function finSubGetDPArray() : Array { return EXTDCLASS.finSubGetDPArray(); }


	// the virtual method attribute.
	public function virSubSetDPArray( a:Array ) { EXTDCLASS.virSubSetDPArray(a); }
	public function virSubGetDPArray() : Array { return EXTDCLASS.virSubGetDPArray(); }


	// the public static method attrbute.
	public function pubStatSubSetDPArray( a:Array ) { FinExtInternalClass.pubStatSubSetDPArray(a); }
	public function pubStatSubGetDPArray() : Array { return FinExtInternalClass.pubStatSubGetDPArray(); }


	// the private static method attribute
	public function testPrivStatSubDPArray( a:Array ) : Array { return EXTDCLASS.testPrivStatSubDPArray(a); }

}
