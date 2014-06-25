/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     general1.js
   Description:  'This tests out some of the functionality on methods on the Array objects'

   Author:       Nick Lerissa
   Date:         Fri Feb 13 09:58:28 PST 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE = 'String:push,unshift,shift';

writeHeaderToLog('Executing script: general1.js');
writeHeaderToLog( SECTION + " "+ TITLE);

var array1 = [];

array1.push(123);            //array1 = [123]
array1.push("dog");          //array1 = [123,dog]
array1.push(-99);            //array1 = [123,dog,-99]
array1.push("cat");          //array1 = [123,dog,-99,cat]
new TestCase( SECTION, "array1.pop()", array1.pop(),'cat');
//array1 = [123,dog,-99]
array1.push("mouse");        //array1 = [123,dog,-99,mouse]
new TestCase( SECTION, "array1.shift()", array1.shift(),123);
//array1 = [dog,-99,mouse]
array1.unshift(96);          //array1 = [96,dog,-99,mouse]
new TestCase( SECTION, "state of array", String([96,"dog",-99,"mouse"]), String(array1));
new TestCase( SECTION, "array1.length", array1.length,4);
array1.shift();              //array1 = [dog,-99,mouse]
array1.shift();              //array1 = [-99,mouse]
array1.shift();              //array1 = [mouse]
new TestCase( SECTION, "array1.shift()", array1.shift(),"mouse");
new TestCase( SECTION, "array1.shift()", "undefined", String(array1.shift()));

test();

