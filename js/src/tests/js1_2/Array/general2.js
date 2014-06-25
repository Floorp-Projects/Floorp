/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     general2.js
   Description:  'This tests out some of the functionality on methods on the Array objects'

   Author:       Nick Lerissa
   Date:         Fri Feb 13 09:58:28 PST 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE = 'String:push,splice,concat,unshift,sort';

writeHeaderToLog('Executing script: general2.js');
writeHeaderToLog( SECTION + " "+ TITLE);

array1 = new Array();
array2 = [];
size   = 10;

// this for loop populates array1 and array2 as follows:
// array1 = [0,1,2,3,4,....,size - 2,size - 1]
// array2 = [size - 1, size - 2,...,4,3,2,1,0]
for (var i = 0; i < size; i++)
{
  array1.push(i);
  array2.push(size - 1 - i);
}

// the following for loop reverses the order of array1 so
// that it should be similarly ordered to array2
for (i = array1.length; i > 0; i--)
{
  array3 = array1.slice(1,i);
  array1.splice(1,i-1);
  array1 = array3.concat(array1);
}

// the following for loop reverses the order of array1
// and array2
for (i = 0; i < size; i++)
{
  array1.push(array1.shift());
  array2.unshift(array2.pop());
}

new TestCase( SECTION, "Array.push,pop,shift,unshift,slice,splice", true,String(array1) == String(array2));
array1.sort();
array2.sort();
new TestCase( SECTION, "Array.sort", true,String(array1) == String(array2));

test();

