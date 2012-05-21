// |reftest| skip-if(xulRuntime.OS=="Linux"&&!isDebugBuild&&!xulRuntime.XPCOMABI.match(/x86_64/)) random -- BigO, slow if Linux 32bit opt
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 56940;
var summary = 'String concat should not be O(N**2)';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

var data = {X:[], Y:[]};
for (var size = 1000; size <= 10000; size += 1000)
{ 
  data.X.push(size);
  data.Y.push(concat(size));
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
reportCompare(true, order < 2, 'BigO ' + order + ' < 2');

function concat(size)
{
  var x = '';
  var y = 'Mozilla Mozilla Mozilla Mozilla ';
  var z = 'goober ';
  var start = new Date();

  for (var loop = 0; loop < size; loop++)
  {
    x = x + y + z + y + z + y + z;
  }
  var stop = new Date();
  return stop - start;
}
