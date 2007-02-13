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

var SECTION = "Definitions";           // provide a document reference (ie, ECMA section)
var VERSION = "AS3";                   // Version of JavaScript or ECMA
var TITLE   = "Testing try block with multiple catch blocks, the sixth catch block catching the type error";  // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone


thisError = "no error";
thisError1="no error";
try{
   throw new RangeError();
   }catch(eo:ReferenceError){
       thisError1 = "This is outer Reference error:"+"  "+eo.toString();
       //print(thisError1);
   }catch(eo1:TypeError){
       thisError1="This is outer TypeError:"+eo1.toString();
       //print(thisError1);
   }catch(eo2:ArgumentError){
       thisError1="This is outer Argument Error:"+eo2.toString();
       //print(thisError1);
   }catch(eo3:URIError){
       thisError1="This is outer URI Error:"+eo3.toString();
   }catch(eo4:EvalError){
       thisError1="This is outer Eval Error:"+eo4.toString();
   }catch(eo5:RangeError){
       thisError1="This is outer Range Error:"+eo5.toString();
       try { 
           throw new TypeError();
           }catch(ei:TypeError){
               thisError="This is Inner Type Error:"+ei.toString();
               //print(thisError);
           }catch(ei1:ReferenceError){
               thisError="Inner reference error:"+ei1.toString();
           }catch(ei2:URIError){
               thisError="This is inner URI Error:"+ei2.toString();
           }catch(ei3:EvalError){
               thisError="This is inner Eval Error:"+ei3.toString();
           }catch(ei4:RangeError){
               thisError="This is inner Range Error:"+ei4.toString();
           }catch(ei5:SecurityError){
               thisError="This is inner Security Error!!!"+ei5.toString();
           }catch(ei6:ArgumentError){
               thisError="This is inner Argument Error"+ei6.toString();
           }finally{
               AddTestCase( "Testing Nested try block with multiple catch block inside the sixth catch block of the outer try block","This is inner finally:This is Inner Type Error:TypeError","This is inner finally:"+thisError );
            }
   }catch(eo6:SecurityError){
       thisError1="This is outer Security Error!!!"+eo6.toString();
   }catch(eo7:Error){
       thisError1="This is outer Error:"+eo7.toString();
   }finally{
       AddTestCase( "Testing Nested try block with multiple catch block inside the sixth catch block of the outer try block", "This is outer finally:This is outer Range Error:RangeError","This is outer finally:"+ thisError1 );
    }
 


test();       // leave this alone.  this executes the test cases and
              // displays results.
