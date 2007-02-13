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
package Importall {

 public class PublicClass {

   var array:Array;
    var boolean:Boolean;
    var date:Date;
    var myFunction:Function;
    var math:Math;
    var number:Number;
    var object:Object;
    var string:String;
    //var simple:Simple;

    public var pubArray:Array; 
    
    public var pubBoolean:Boolean;
    public var pubDate:Date;
    public var pubFunction:Function;
    public var pubMath:Math;
    public var pubNumber:Number;
    public var pubObject:Object;
    public var pubString:String;
    //public var pubSimple:Simple; 

  
    // *******************
    // public methods
    // *******************

    public function setPubArray( a:Array ) { pubArray = a; }
    public function setPubBoolean( b:Boolean ) { pubBoolean = b; }
    public function setPubDate( d:Date ) { pubDate = d; }
    public function setPubFunction( f:Function ) { pubFunction = f; }
    public function setPubMath( m:Math ) { pubMath = m; }
    public function setPubNumber( n:Number ) { pubNumber = n; }
    public function setPubObject( o:Object ) { pubObject = o; }
    public function setPubString( s:String ) { pubString = s; }
    //public function setPubSimple( s:Simple ) { pubSimple = s; }

    public function getPubArray() : Array { return this.pubArray; }
    public function getPubBoolean() : Boolean { return this.pubBoolean; }
    public function getPubDate() : Date { return this.pubDate; }
    public function getPubFunction() : Function { return this.pubFunction; }
    public function getPubMath() : Math { return this.pubMath; }
    public function getPubNumber() : Number { return this.pubNumber; }
    public function getPubObject() : Object { return this.pubObject; }
    public function getPubString() : String { return this.pubString; }
    //public function getPubSimple() : Simple { return this.pubSimple; }

  }
}


var SECTION = "Directives";       				// provide a document reference (ie, ECMA section)
var VERSION = "ActionScript 3.0";  				// Version of JavaScript or ECMA
var TITLE   = "Import all public names in a package";       	// Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                					// leave this alone


import Importall.*

var obj = new PublicClass();

var arr = new Array(1, 2, 3);
date = new Date(0);
func = new Function();
num = new Number();
str = new String("test");

// ********************************************
// access all public method  
// ********************************************

AddTestCase( "obj.setPubArray(arr), obj.pubArray", arr, (obj.setPubArray(arr), obj.pubArray) );
AddTestCase( "obj.setPubBoolean(true), obj.pubBoolean", true, (obj.setPubBoolean(true), obj.pubBoolean) );
AddTestCase( "obj.setPubFunction(func), obj.pubFunction", func, (obj.setPubFunction(func), obj.pubFunction) );
AddTestCase( "obj.setPubNumber(num), obj.pubNumber", num, (obj.setPubNumber(num), obj.pubNumber) );
AddTestCase( "obj.setPubObject(obj), obj.pubObject", obj, (obj.setPubObject(obj), obj.pubObject) );
AddTestCase( "obj.setPubString(str), obj.pubString", str, (obj.setPubString(str), obj.pubString) );

// ********************************************
// access public property from outside
// the class
// ********************************************

AddTestCase( "Access public property from outside the class", arr, (obj.pubArray = arr, obj.pubArray) );


/*===========================================================================*/


test();       // leave this alone.  this executes the test cases and
              // displays results.
