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
package UserDefinedErrorsPackageTryBlockOutside
{
    public class BoxDimensionException extends Error
    {
        public function BoxDimensionException(boxErrMsg:String)
        {
            super(boxErrMsg);
                                                     
        }
      
     }
     public class BoxUnderzeroException extends BoxDimensionException
     {
      
        public function BoxUnderzeroException(boxErrMsg2:String)
        {
            super(boxErrMsg2);
        }
      }

      public class BoxOverflowException extends BoxDimensionException
      {
          public function BoxOverflowException(boxErrMsg3:String)
          {
              super(boxErrMsg3);
          }
      }

      public class Box
      {
          private var width:Number;
     
          public function setWidth(w):Boolean
          {
             var k:String = decodeURI("!@#$%^&*()<>?");
             eval(m);
             var arr:Array=new Array(-10);
             var MyString:String = MyString+1;
             var errmsg:String="Illegal Box Dimension specified";
             var errmsg2:String="Box dimensions should be greater than 0";
             var errmsg3:String="Box dimensions must be less than Number.MAX_VALUE";
             if (isNaN(w)){
                  throw new BoxDimensionException(errmsg);
             }else if (w<= 0){
                  throw new BoxUnderzeroException(errmsg2);
             }else if (w>Number.MAX_VALUE){
                  throw new BoxOverflowException(errmsg3);
              }
     
             width = w;
            
           }
       }
}
package TryCatchBlockUserWithBuiltInExceptions
{
    import UserDefinedErrorsPackageTryBlockOutside.*;
    public class TryAndCatchBlockWithUserDefinedErrors
    {
        var b:Box = new Box();
        var someWidth:Number=(Number.MAX_VALUE)*10;
        var thisError = "no error";
        var thisError1 = "no error";
        var thisError2 = "no error";
        var thisError3 = "no error";
        var thisError4 = "no error";
        var thisError5 = "no error";
        var thisError6 = "no error";
        var thisError11 = "no error";
        var thisError8 = "no error";
        var thisError9 = "no error";
        var thisError10 ="no error";
        public function MyTryThrowCatchFunction():void
        {
            try {
                b.setWidth(someWidth);
                }catch(e:BoxOverflowException){
                     thisError = e.message;
                     //trace("BoxOverflowException:"+thisError);
                }catch(e1:BoxUnderzeroException){
                     thisError1=e1.message;
                     //trace("BoxUnderzeroException:"+thisError1);
                }catch(e2:BoxDimensionException){
                     thisError2 = e2.message;
                     //trace("BoxDimensionException Occurred:"+thisError2);
                }catch(e3:ReferenceError){
                     thisError3=e.toString();
                     //print(thisError3);
                }catch(e4:TypeError){
                     thisError4=e4.toString();
                     //print(thisError4)
                }catch(e5:ArgumentError){
                     thisError5=e5.toString();
                     //print(thisError5)
                }catch(e6:URIError){
                     thisError6=e6.toString();
                     //print(thisError6)
                }catch(e8:UninitializedError){
                     thisError8=e8.toString();
                     //print(thisError8)
                }catch(e9:EvalError){
                     thisError9=e9.toString();
                     //print(thisError9)
                }catch(e10:RangeError){
                     thisError10=e10.toString();
                     //print(thisError10)
                }catch(e11:Error){
                     //print(e11.toString());
                     thisError11=e11.toString();
                }finally{//print("This Error is:"+thisError);
                     AddTestCase( "Testing try block and multiple catch blocks with       custom error classes", "no error",thisError ); 
                     AddTestCase( "Testing catch block with type error", 
                           "no error",typeError(thisError4) );
                     AddTestCase( "Testing catch block with Argument Error",                                        "no error" ,thisError5);
                     AddTestCase( "Testing catch block with URIError", 
                           "URIError: Error #1052",uriError(thisError6));    
                     AddTestCase( "Testing catch block with Eval Error", 
                           "no error" ,thisError9);                                       AddTestCase( "Testing catch block with Range Error", 
                           "no error",thisError10);
                     AddTestCase( "Testing catch block with Error", "no error"                                          ,thisError11);     
                 }
          }
     }
}         
