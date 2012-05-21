// |reftest| random -- BigO
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 347306;
var summary = 'decompilation should not be O(N**2)';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

var data = {X:[], Y:[]};
for (var size = 1000; size <= 10000; size += 1000)
{ 
  data.X.push(size);
  data.Y.push(testSource(size));
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

function testSource(n)
{
  var funtext = "";
 
  for (var i=0; i<n; ++i)
    funtext += "alert(" + i + "); ";

  var fun = new Function(funtext);

  var start = new Date();
 
  var s = fun + "";
 
  var end = new Date();

  print("Size: " + n + ", Time: " + (end - start) + " ms");

  return end - start;
}
