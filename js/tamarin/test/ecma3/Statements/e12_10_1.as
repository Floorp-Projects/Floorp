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

    var SECTION = "12.10-1";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "The with statment";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();

    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    // although the scope chain changes, the this value is immutable for a given
    // execution context.

    var x;
    with( new Number() )
    { 
        x = this +'';
    }
	array[item++] = new TestCase( SECTION,
                                "with( new Number() ) { this +'' }",
                                "[object global]",
                                 x);
    // the object's functions and properties should override those of the
    // global object.

    var MYOB = new WithObject(true); with (MYOB) { y=parseInt() }
    array[item++] = new TestCase(
                        SECTION,
                        "var MYOB = new WithObject(true); with (MYOB) { parseInt() }",
                        true,
                        y );

    var MYOB = new WithObject(false); 
    with (MYOB)
    { 
        z = NaN;
    }
    array[item++] = new TestCase(
                        SECTION,
                        "var MYOB = new WithObject(false); with (MYOB) { NaN }",
                        false,
                        z );

    var MYOB = new WithObject(NaN); 
    with (MYOB) { r = Infinity }
    array[item++] = new TestCase(
                        SECTION,
                        "var MYOB = new WithObject(NaN); with (MYOB) { Infinity }",
                        Number.NaN,
                         r);

    var MYOB = new WithObject(false); with (MYOB) { }
    array[item++] = new TestCase(
                        SECTION,
                        "var MYOB = new WithObject(false); with (MYOB) { }; Infinity",
                        Number.POSITIVE_INFINITY,
                        Infinity );


    var MYOB = new WithObject(0); with (MYOB) { delete Infinity; }
    array[item++] = new TestCase(
                        SECTION,
                        "var MYOB = new WithObject(0); with (MYOB) { delete Infinity; Infinity }",
                        Number.POSITIVE_INFINITY,
                        Infinity );
     

    // let us leave the with block via a break.

    var MYOB = new WithObject(false); while (true) { with (MYOB) { MYOB = Infinity; break; } }
    array[item++] = new TestCase(
                        SECTION,
                        "var MYOB = new WithObject(false); while (true) { with (MYOB) { Infinity; break; } }",
                        false,
                        MYOB );
    
    var MYOB = new WithObject(true); 
    with (MYOB) { MYOB = Infinity;f(); }
    array[item++] = new TestCase(
                        SECTION,
                        "var MYOB = new WithObject(true); with (MYOB) { Infinity }",
                        true,
                         MYOB);
function f(a,b){}

     

    return ( array );
}
function WithObject( value ) {
    this.prop1 = 1;
    this.prop2 = new Boolean(true);
    this.prop3 = "a string";
    this.value = value;

    // now we will override global functions

    //this.parseInt = new Function( "return this.value" );
	this.parseInt = function(){return this.value;}	
    this.NaN = value;
    this.Infinity = value;
    this.unescape = function(){return this.value;}
    this.escape   = function(){return this.value;}
    this.eval     = function(){return this.value;}
    this.parseFloat = function(){return this.value;}
    this.isNaN      = function(){return this.value;}
    this.isFinite   = function(){return this.value;}
}
