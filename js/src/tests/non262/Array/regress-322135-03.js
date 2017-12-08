// |reftest| skip -- slow
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 322135;
var summary = 'Array.prototype.splice on Array with length 2^32-1';
var actual = 'Completed';
var expect = 'Completed';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
printStatus('This bug passes if it does not cause an out of memory error');
printStatus('Other issues related to array length are not tested.');
 
var length = 4294967295;
var array = new Array(length);
var array1 = ['Kibo'];
var array;

try
{
  array.splice(0, 0, array1);
}
catch(ex)
{
  printStatus(ex.name + ': ' + ex.message);
}
reportCompare(expect, actual, summary + ': RangeError');

//expect = 'Kibo';
//actual = array[0];
//reportCompare(expect, actual, summary + ': element prepended');

//expect = length;
//actual = array.length;
//reportCompare(expect, actual, summary + ': array length unchanged');
