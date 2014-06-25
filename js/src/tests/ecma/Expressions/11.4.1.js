/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          11.4.1.js
   ECMA Section:       11.4.1 the Delete Operator
   Description:        returns true if the property could be deleted
   returns false if it could not be deleted
   Author:             christine@netscape.com
   Date:               7 july 1997

*/


var SECTION = "11.4.1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "The delete operator";

writeHeaderToLog( SECTION + " "+ TITLE);

//    new TestCase( SECTION,   "x=[9,8,7];delete(x[2]);x.length",         2,             eval("x=[9,8,7];delete(x[2]);x.length") );
//    new TestCase( SECTION,   "x=[9,8,7];delete(x[2]);x.toString()",     "9,8",         eval("x=[9,8,7];delete(x[2]);x.toString()") );
new TestCase( SECTION,   "x=new Date();delete x;typeof(x)",        "undefined",    eval("x=new Date();delete x;typeof(x)") );

//    array[item++] = new TestCase( SECTION,   "delete(x=new Date())",        true,   delete(x=new Date()) );
//    array[item++] = new TestCase( SECTION,   "delete('string primitive')",   true,   delete("string primitive") );
//    array[item++] = new TestCase( SECTION,   "delete(new String( 'string object' ) )",  true,   delete(new String("string object")) );
//    array[item++] = new TestCase( SECTION,   "delete(new Number(12345) )",  true,   delete(new Number(12345)) );
new TestCase( SECTION,   "delete(Math.PI)",             false,   delete(Math.PI) );
//    array[item++] = new TestCase( SECTION,   "delete(null)",                true,   delete(null) );
//    array[item++] = new TestCase( SECTION,   "delete(void(0))",             true,   delete(void(0)) );

// variables declared with the var statement are not deletable.

var abc;
new TestCase( SECTION,   "var abc; delete(abc)",        false,   delete abc );

new TestCase(   SECTION,
                "var OB = new MyObject(); for ( p in OB ) { delete p }",
                true,
                eval("var OB = new MyObject(); for ( p in OB ) { delete p }") );

test();

function MyObject() {
  this.prop1 = true;
  this.prop2 = false;
  this.prop3 = null;
  this.prop4 = void 0;
  this.prop5 = "hi";
  this.prop6 = 42;
  this.prop7 = new Date();
  this.prop8 = Math.PI;
}
