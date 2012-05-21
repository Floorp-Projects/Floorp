// |reftest| random -- BigO
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 157334;
var summary = 'String concat should not be O(N**2)';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

var data = {X:[], Y:[]};
for (var size = 1000; size < 10000; size += 1000)
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
  var c='qwertyuiop';
  var x='';
  var y='';
  var loop;

  for (loop = 0; loop < size; loop++)
  {
    x += c;
  }

  y = x + 'y';
  x = x + 'x';

  var start = new Date();
  for (loop = 0; loop < 1000; loop++)
  {
    var z = x + y + loop.toString();
  }
  var stop = new Date();
  return stop - start;
}
