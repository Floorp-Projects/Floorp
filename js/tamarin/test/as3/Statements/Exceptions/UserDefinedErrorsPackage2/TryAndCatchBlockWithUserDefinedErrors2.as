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
package UserDefinedErrorsPackage2
{
    class BoxDimensionException extends Error
    {
        public function BoxDimensionException(boxErrMsg:String)
        {
            super(boxErrMsg);
                                                     
        }
             
    }
    class BoxUnderzeroException extends BoxDimensionException
    {
        public function BoxUnderzeroException(boxErrMsg2:String)
         {
            super(boxErrMsg2);
                                                                 
         }
    }

    class BoxOverflowException extends BoxDimensionException
    {
        public function BoxOverflowException(boxErrMsg3:String)
          {
             super(boxErrMsg3);
                                                            
          }
    }

    class Box
    {
        private var width:Number;
     
        public function setWidth(w):Boolean
           {
               var errmsg:String="Illegal Box Dimension specified";
               var errmsg2:String="Box dimensions should be greater than 0";
               var errmsg3:String="Box dimensions must be less than Number.MAX_VALUE";
               if (isNaN(w))
               {
                   throw new BoxDimensionException(errmsg);
               }else if (w<= 0)
                {
                     throw new BoxUnderzeroException(errmsg2);
                }else if (w>Number.MAX_VALUE)
                 {
                      throw new BoxOverflowException(errmsg3);
                 }
     
               width = w;
            
            }
     }

     public class TryAndCatchBlockWithUserDefinedErrors2
     {
         var b:Box = new Box();
         var someWidth:Number=-10;
         thisError = "no error";

         public function MyTryThrowCatchFunction():void
        {
            try {
                b.setWidth(someWidth);
                }catch(e:BoxOverflowException){
                    thisError = e.message;
                    //trace("BoxOverflowException:"+thisError);
                }catch(e1:BoxUnderzeroException){
                    thisError=e1.message;
                   //trace("BoxUnderzeroException:"+thisError);
                }catch(e2:BoxDimensionException){
                    thisError = e2.message;
                    //trace("BoxDimensionException Occurred:"+thisError);
                }catch(e3:Error){
                    thisError=e3.message;
                    //trace("An error occurred:"+e3.toString());
                }finally{
                    AddTestCase( "Testing try block and multiple catch blocks with custom error classes", "Box dimensions should be greater than 0",thisError );   
                 }
         }
      }
}
