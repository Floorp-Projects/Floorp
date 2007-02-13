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
package testInternalClassWithParamCons{

 internal class IntClassWithStrParamCons{
             public var x:String;
             public var myarr:Array;
             public var myObj:publicClassCons;
              
  function IntClassWithStrParamCons(a:String,b:Boolean,c:Array,d:publicClassCons,e:DefaultClass)

                                                                   {
                                                                     x=a;
                                                                     y=b;
                                                                     myarr=c;
                                                                     myObj=d;              
                                                                            
                                                                                }
                                        

                                        public function myString():String{
                                                              
                                                              return x;
                                                                         }
                                        public function myBoolean():Boolean{
                                                              
                                                              return y;
                                                                           }
                                        public function myarray():Array{
                                                             
                                                             return myarr;
                                                                       }
                                        
                                        public function myAdd():Number{
                                                              return myObj.Add();
                                                                      }                               


                                          }




public class publicClassCons{

      private var x:Number=4;
      private var y:Number=5;


      public function publicClassCons(){
                                        }

      public function Add(){
             var z = x+y;
             return z;

                           
                                }
                                                }
class DefaultClass{}

public class wrapIntClassWithStrParamCons{
   var x = "test"; 
   var y:Boolean = true;
   var myArray:Array = new Array(4,6,5);
   var pbClCons:publicClassCons = new publicClassCons(); 
   var MyDefaultClass:DefaultClass;
   var ICWSPS:IntClassWithStrParamCons = new IntClassWithStrParamCons(x,y,myArray,pbClCons,MyDefaultClass);
             public function myArray1():Array{
                                             return myArray;
                                             }

             public function wrapmyString():String{
                                               var w:String = ICWSPS.myString();
                                                              
                                               return w;
                                                }
             public function wrapmyBoolean():Boolean{
                                                var H:Boolean = ICWSPS.myBoolean();             
                                                return H;
                                                }
                                        
             public function wrapmyarray():Array{
                                                var I:Array = ICWSPS.myarray();            
                                                return I;
                                                 }
                                        
              public function wrapmyAdd():Number{
                                                var J:Number = ICWSPS.myAdd();
                                            return J;
                                            }                               

                                         }


                         
                                                        }
