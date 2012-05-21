// |reftest| random -- BigO
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 99120;
var summary = 'sort() should not be O(N^2) on nearly sorted data';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
var data = {X:[], Y:[]};
for (var size = 5000; size <= 15000; size += 1000)
{ 
  data.X.push(size);
  data.Y.push(testSort(size));
  gc();
}

var order = BigO(data);

var msg = '';
for (var p = 0; p < data.X.length; p++)
{
  msg += '(' + data.X[p] + ', ' + data.Y[p] + '); ';
}
printStatus(msg);
printStatus('Order: ' + order);
reportCompare(true, order < 2 , 'BigO ' + order + ' < 2');

function testSort(size)
{
  var arry = new Array(size);
  for (var i = 0; i < size; i++)
  {
    arry[i] = i + '';
  }
  var start = new Date();
  arry.sort();
  var stop = new Date();
  return stop - start;
}
