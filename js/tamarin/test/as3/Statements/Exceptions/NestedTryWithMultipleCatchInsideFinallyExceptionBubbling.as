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


import NestedTryWithMultipleCatchInsideFinallyExceptionBubbling.*;

var SECTION = "Definitions";           // provide a document reference (ie, ECMA section)
var VERSION = "AS3";                   // Version of JavaScript or ECMA
var TITLE   = "Testing nested try block with multiple catch blocks, inner try and catch blocks inside finally block of the outer try and multiple catch blocks";  // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone


var z:NestedTryWithMultipleCatchInsideFinally = new NestedTryWithMultipleCatchInsideFinally();
thisError = "no error";
thisError1 = "no error";       
try{   
   z.NestedTryWithMultipleCatchInsideFinallyFunction();
   }catch(eo:ReferenceError){
         thisError=eo.toString();
   }catch(eo1:TypeError){
         thisError="This is outer Type Error:"+eo1.toString();//print(thisError);
   }catch(eo2:ArgumentError){
         thisError=eo2.toString();
   }catch(eo3:URIError){
         thisError=eo3.toString()
   }catch(eo4:UninitializedError){
         thisError=eo4.toString();
   }catch(eo5:EvalError){
         thisError=eo5.toString();
   }catch(eo6:RangeError){
         thisError=eo6.toString();
   }catch(eo7:DefinitionError){
       thisError="This is Definition Error";
   }catch(eo8:SyntaxError){
         thisError="This is Syntax Error";
   }catch(eo9:VerifyError){
         thisError="This is Verify Error";
   }catch(eo10:Error){//print(e10.toString());
         thisError=eo10.toString();
   }finally{//print(eo1.toString());
AddTestCase( "Testing catch block with Type Error", "This is outer Type Error:TypeError" ,thisError);
    }




test();       // leave this alone.  this executes the test cases and
              // displays results.
