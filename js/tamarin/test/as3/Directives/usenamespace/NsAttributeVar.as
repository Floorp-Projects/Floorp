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


package P {
 
     public namespace nsPublic1    	
     public namespace nsPublic2    
     internal namespace nsInternal1    	
     internal namespace nsInternal2    
     

	public class Game{

 protected namespace nsProtected1    	
  protected namespace nsProtected2 
  private namespace nsPrivate1    	
     private namespace nsPrivate2  
        
             nsPublic1 var x:int = 10; 
             nsPublic2 var x:String = "team1";
             nsInternal1 var y:int = 10; 
             nsInternal2 var y:String = "team1";
	     nsPrivate1 var z:int = 10; 
             nsPrivate2 var z:String = "team1";
             nsProtected1 var a:int = 10; 
             nsProtected2 var a:String = "team1";
	    
             public function accintvar1(){return nsInternal1::y} //public function to access internal variable
             public function accintvar2(){return nsInternal2::y} //public function to access internal variable
             public function accprivvar1(){return nsPrivate1::z} //public function to access private variable
             public function accprivvar2(){return nsPrivate2::z} //public function to access private variable
             public function accprotvar1(){return nsProtected1::a} //public function to access protected variable
             public function accprotvar2(){return nsProtected2::a} //public function to access protected variable

     }
 }

 
var SECTION = "Directives";       // provide a document reference (ie, Actionscript section)
var VERSION = "AS 3.0";        // Version of ECMAScript or ActionScript 
var TITLE   = "Public_Private_Internal_Protected_Namespace as variable attribute";       // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone





import P.*;

var game = new Game();



AddTestCase( "Public Namespace as variable attribute x = 10", 10, game.nsPublic1::x);
AddTestCase( "Public Namespace as variable attribute x = 'team1'", "team1", game.nsPublic2::x);
AddTestCase( "Internal Namespace as variable attribute y = 10", 10, game.accintvar1());
AddTestCase( "Internal Namespace as variable attribute y = 'team1'", "team1", game.accintvar2());
AddTestCase( "Private Namespace as variable attribute z = 10", 10, game.accprivvar1());
AddTestCase( "Private Namespace as variable attribute z = 'team1'", "team1", game.accprivvar2());
AddTestCase( "Protected Namespace as variable attribute a = 10", 10, game.accprotvar1());
AddTestCase( "Protected Namespace as variable attribute a = 'team1'", "team1", game.accprotvar2());

test();       // leave this alone.  this executes the test cases and
              // displays results.
