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

package DefaultProtClass {

  class DefaultProtClassInner {

    protected var protArray:Array;
    protected var protBoolean:Boolean;
    protected var protDate:Date;
    protected var protFunction:Function;
    protected var protMath:Math;
    protected var protNumber:Number;
    protected var protObject:Object;
    protected var protString:String;
    //protected var protSimple:Simple;

    protected static var protStatArray:Array;
    protected static var protStatBoolean:Boolean;
    protected static var protStatDate:Date;
    protected static var protStatFunction:Function;
    protected static var protStatMath:Math;
    protected static var protStatNumber:Number;
    protected static var protStatObject:Object;
    protected static var protStatString:String;
    //protected static var protStatSimple:Simple;

    // *****************************
    // to be overloaded
    // *****************************

    protected var protOverLoadVar;
    protected static var protStatOverLoadVar;

    // ****************
    // constructor
    // ****************

    function DefaultClassProt() {
    }

    // *******************
    // protected methods
    // *******************

    protected function setProtArray( a:Array ) { protArray = a; }
    protected function setProtBoolean( b:Boolean ) { protBoolean = b; }
    protected function setProtDate( d:Date ) { protDate = d; }
    protected function setProtFunction( f:Function ) { protFunction = f; }
    protected function setProtMath( m:Math ) { protMath = m; }
    protected function setProtNumber( n:Number ) { protNumber = n; }
    protected function setProtObject( o:Object ) { protObject = o; }
    protected function setProtString( s:String ) { protString = s; }
    //protected function setProtSimple( s:Simple ) { protSimple = s; }

    protected function getProtArray() : Array { return this.protArray; }
    protected function getProtBoolean() : Boolean { return this.protBoolean; }
    protected function getProtDate() : Date { return this.protDate; }
    protected function getProtFunction() : Function { return this.protFunction; }
    protected function getProtMath() : Math { return this.protMath; }
    protected function getProtNumber() : Number { return this.protNumber; }
    protected function getProtObject() : Object { return this.protObject; }
    protected function getProtString() : String { return this.protString; }
    //protected function getProtSimple() : Simple { return this.protSimple; }

    // **************************
    // protected static methods
    // **************************

    protected static function setProtStatArray(a:Array) { protStatArray=a; }
    protected static function setProtStatBoolean( b:Boolean ) { protStatBoolean = b; }

    protected static function getProtStatArray() { return protStatArray; }

    // ***************************
    // to be overloaded
    // ***************************

    protected function protOverLoad() { return "This is the parent class"; }
    protected static function protStatOverLoad() { return "This is the parent class"; }
  }

  public class DefaultProtClass extends DefaultProtClassInner { }

}
