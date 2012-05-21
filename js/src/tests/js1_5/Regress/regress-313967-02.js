// |reftest| random -- BigO
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 313967;
var summary = 'Compile time of N functions should be O(N)';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

function F(x, y) {
  var a = x+y;
  var b = x-y;
  var c = a+b;
  return ( 2*c-1 );
}

function test(N)
{
  var src = F.toSource(-1)+"\n";
  src = Array(N + 1).join(src);
  var counter = 0;
  src = src.replace(/F/g, function() { ++counter; return "F"+counter; })
    var now = Date.now;
  var t = now();
  eval(src);
  return now() - t;
}


var data = {X:[], Y:[]};
for (var i = 0; i != 10; ++i)
{ 
  data.X.push(i);
  data.Y.push(test(1000 + i * 2000));
  gc();
}

var order = BigO(data);

var msg = '';
for (var p = 0; p < data.X.length; p++)
{
  msg += '(' + data.X[p] + ', ' + data.Y[p] + '); ';
}

print(msg);
 
reportCompare(true, order < 2, summary + ': BigO ' + order + ' < 2');
