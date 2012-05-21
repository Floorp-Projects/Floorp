// |reftest| random -- BigO
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 314890;
var summary = 'String == should short circuit for object identify';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

var s1 = 'x';
var s2;
var count = 0;

var data = {X:[], Y:[]};
for (var power = 0; power < 20; power++)
{ 
  s1 = s1 + s1;
  s2 = s1;

  data.X.push(s1.length);
  var start = new Date();
  for (var count = 0; count < 1000; count++)
  {
    if (s1 == s2)
    {
      ++count;
    }
  }
  var stop = new Date();
  data.Y.push(stop - start);
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
reportCompare(true, order < 1, 'BigO ' + order + ' < 1');

reportCompare(expect, actual, summary);

