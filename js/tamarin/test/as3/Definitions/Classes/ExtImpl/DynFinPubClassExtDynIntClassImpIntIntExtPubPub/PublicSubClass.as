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

package DynFinPubClassExtDynIntClassImpIntIntExtPubPub{
        public interface PublicInt{
                                  function MyString():String;
                                  }
        public interface PublicInt2{
                                  function MyString():String;
                                  function MyNegInteger():int;
                                   }
        internal interface PublicInt3 extends PublicInt,PublicInt2{
                                  function MyUnsignedInteger():uint;
                                  function MyNegativeInteger():int;
                                                                 }
        dynamic internal class InternalSuperClass{
                                   public function MySuperBoolean():Boolean{return true;}
                                   internal function MySuperNumber():Number{return 10;}
                            public static function MySuperStaticDate():Date {return new Date(0);}

                            
                                     }
                                              
        dynamic final public class PublicSubClass extends InternalSuperClass implements PublicInt3{
                                                           
                                 public function MyString():String{
                                                                    return "Hi!";
                                                                    }
                                 
                                 public function MyNegInteger():int{
                                                                    var a:int = -100;
                                                                    return a;
                                                                       }
                                 public function MyUnsignedInteger():uint{
                                                                    var postint:uint =100;
                                                                    return postint;
                                                                  }
                                  public function MyNegativeInteger():int{
                                                                    var negint:int = -100000;
                                                                    return negint;
                                                                           }
                                  public function RetMySuperNumber():Number{return MySuperNumber();}
                                  public function RetMySuperBoolean():Boolean{return MySuperBoolean();}
                                  public function RetMySuperStaticDate():Date{return MySuperStaticDate();}
                                                            }

        public class InternalInterfaceAccessor{
                var k = new PublicSubClass();

                var PubInt3:PublicInt3 = k;
                var PubInttwo:PublicInt2=k;
                public function RetMyNegInteger():int{
                                                      return PubInt3.MyNegInteger();
                                                    }
                public function RetMyNegInteger2():int{
                                                      return PubInttwo.MyNegInteger();
                                                      }
                public function RetMyUnsignedInteger():uint{
                                                      return PubInt3.MyUnsignedInteger();
                                                            }
                public function RetMyNegativeInteger():int{
                                                 return k.PublicInt3::MyNegativeInteger()}
                                                 }
                                            }

                                                         
