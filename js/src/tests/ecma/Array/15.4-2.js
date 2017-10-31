/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.4-2.js
   ECMA Section:       15.4 Array Objects

   Description:        Whenever a property is added whose name is an array
   index, the length property is changed, if necessary,
   to be one more than the numeric value of that array
   index; and whenever the length property is changed,
   every property whose name is an array index whose value
   is not smaller  than the new length is automatically
   deleted.  This constraint applies only to the Array
   object itself, and is unaffected by length or array
   index properties that may be inherited from its
   prototype.

   Author:             christine@netscape.com
   Date:               28 october 1997

*/
var SECTION = "15.4-2";
var TITLE   = "Array Objects";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( "var arr=new Array();  arr[Math.pow(2,16)] = 'hi'; arr.length",     
              Math.pow(2,16)+1,  
              eval("var arr=new Array();  arr[Math.pow(2,16)] = 'hi'; arr.length") );

new TestCase( "var arr=new Array();  arr[Math.pow(2,30)-2] = 'hi'; arr.length",   
              Math.pow(2,30)-1,  
              eval("var arr=new Array();  arr[Math.pow(2,30)-2] = 'hi'; arr.length") );

new TestCase( "var arr=new Array();  arr[Math.pow(2,30)-1] = 'hi'; arr.length",   
              Math.pow(2,30),    
              eval("var arr=new Array();  arr[Math.pow(2,30)-1] = 'hi'; arr.length") );

new TestCase( "var arr=new Array();  arr[Math.pow(2,30)] = 'hi'; arr.length",     
              Math.pow(2,30)+1,  
              eval("var arr=new Array();  arr[Math.pow(2,30)] = 'hi'; arr.length") );


new TestCase( "var arr=new Array();  arr[Math.pow(2,31)-2] = 'hi'; arr.length",   
              Math.pow(2,31)-1,  
              eval("var arr=new Array();  arr[Math.pow(2,31)-2] = 'hi'; arr.length") );

new TestCase( "var arr=new Array();  arr[Math.pow(2,31)-1] = 'hi'; arr.length",   
              Math.pow(2,31),    
              eval("var arr=new Array();  arr[Math.pow(2,31)-1] = 'hi'; arr.length") );

new TestCase( "var arr=new Array();  arr[Math.pow(2,31)] = 'hi'; arr.length",     
              Math.pow(2,31)+1,  
              eval("var arr=new Array();  arr[Math.pow(2,31)] = 'hi'; arr.length") );

new TestCase( "var arr = new Array(0,1,2,3,4,5); arr.length = 2; String(arr)",    
              "0,1",             
              eval("var arr = new Array(0,1,2,3,4,5); arr.length = 2; String(arr)") );

new TestCase( "var arr = new Array(0,1); arr.length = 3; String(arr)",            
              "0,1,",            
              eval("var arr = new Array(0,1); arr.length = 3; String(arr)") );

test();

