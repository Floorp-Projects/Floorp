// |reftest| random -- BigO
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 311414;
var summary = 'RegExp captured tail match should be O(N)';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
function q1(n) {
  var c = [];
  c[n] = 1;
  c = c.join(" ");
  var d = Date.now();
  var e = c.match(/(.*)foo$/);
  var f = Date.now();
  return (f - d);
}

function q2(n) {
  var c = [];
  c[n] = 1;
  c = c.join(" ");
  var d = Date.now();
  var e = /foo$/.test(c) && c.match(/(.*)foo$/);
  var f = Date.now();
  return (f - d);
}

var data1 = {X:[], Y:[]};
var data2 = {X:[], Y:[]};

for (var x = 500; x < 5000; x += 500)
{
  var y1 = q1(x);
  var y2 = q2(x);
  data1.X.push(x);
  data1.Y.push(y1);
  data2.X.push(x);
  data2.Y.push(y2);
  gc();
}

var order1 = BigO(data1);
var order2 = BigO(data2);

var msg = '';
for (var p = 0; p < data1.X.length; p++)
{
  msg += '(' + data1.X[p] + ', ' + data1.Y[p] + '); ';
}
printStatus(msg);
printStatus('Order: ' + order1);
reportCompare(true, order1 < 2 , summary + ' BigO ' + order1 + ' < 2');

msg = '';
for (var p = 0; p < data2.X.length; p++)
{
  msg += '(' + data2.X[p] + ', ' + data2.Y[p] + '); ';
}
printStatus(msg);
printStatus('Order: ' + order2);
reportCompare(true, order2 < 2 , summary + ' BigO ' + order2 + ' < 2');
