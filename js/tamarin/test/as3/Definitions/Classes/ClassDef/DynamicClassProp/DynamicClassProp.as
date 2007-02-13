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
 
 
 
 package DynamicClassPropPackage {
 
      
   public dynamic class DynamicClassProp {
   
       var array:Array;
       
       private var privDate:Date;
       
       public var pubBoolean:Boolean;
              
       static var statFunction:Function;
       
       private static var privStatString:String;
       
       public static var pubStatObject:Object;
       
       internal var finNumber:Number;
       
       public var pubFinArray:Array;
       
       internal static var finStatNumber:Number;
              
          
         
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
        
        
        // *******************
	// private methods
	// *******************
	   
       private function getPrivDate() : Date { return privDate; }
       private function setPrivDate( d:Date ) { privDate = d; }
       
       // wrapper function
       
       public function testGetSetPrivDate(d:Date) : Date {
       		setPrivDate(d);
       		return getPrivDate();
        }
       
       
   
       // *******************
       // public methods
       // *******************
   
       public function setPubBoolean( b:Boolean ) { pubBoolean = b; }
       public function getPubBoolean() : Boolean { return pubBoolean; }
       
          
   
       // *******************
       // static methods
       // *******************
   
       static function setStatFunction(f:Function) { statFunction = f; }
       static function getStatFunction() { return statFunction; }
       
       // wrapper function
       
       public function testGetSetStatFunction(f:Function) : Function {
       		setStatFunction(f);
       		return getStatFunction();
        }
       
       
   
       // **************************
       // private static methods
       // **************************
   
       private static function setPrivStatString(s:String) { privStatString = s; }
       private static function getPrivStatString() { return privStatString; }
       
       // wrapper function
       
       public function testGetSetPrivStatString(s:String) : String {
       		setPrivStatString(s);
       		return getPrivStatString();
        }
       
       
       
       // **************************
       // public static methods
       // **************************
   
       public static function setPubStatObject(o:Object) { pubStatObject = o; }
       public static function getPubStatObject() { return pubStatObject; }


   
       // *******************
       // final methods
       // *******************
   
       final function setFinNumber(n:Number) { finNumber = n; }
       final function getFinNumber() { return finNumber; }
       
       // wrapper function
       
       public function testGetSetFinNumber(n:Number) : Number {
       		setFinNumber(n);
       		return getFinNumber();
        }
       
       
       
       // *******************
       // public final methods
       // *******************
   
       public final function setPubFinArray(a:Array) { pubFinArray = a; }
       public final function getPubFinArray() { return pubFinArray; }
       
       
       

   
   
   }
   
}
