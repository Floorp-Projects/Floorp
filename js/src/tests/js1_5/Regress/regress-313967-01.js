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


var times = [];
for (var i = 0; i != 10; ++i) {
  var time = test(1000 + i * 2000);
  times.push(time);
  gc();
}

var r = calculate_least_square_r(times);

print('test times: ' + times);

print("r coefficient for the estimation that eval is O(N) where N is number\n"+
      "of functions in the script: "+r);

print( r < 0.97 ? "Eval time growth is not linear" : "The growth seems to be linear");

reportCompare(true, r >= 0.97, summary);

function calculate_least_square_r(array)
{
  var sum_x = 0;
  var sum_y = 0;
  var sum_x_squared = 0;
  var sum_y_squared = 0;
  var sum_xy = 0;
  var n  = array.length;
  for (var i = 0; i != n; ++i) {
    var x = i;
    var y = array[i];
    sum_x += x;
    sum_y += y;
    sum_x_squared += x*x;
    sum_y_squared += y*y;
    sum_xy += x*y;
  }
  var x_average = sum_x / n;
  var y_average = sum_y / n;
  var sxx = sum_x_squared - n * x_average * x_average;
  var syy = sum_y_squared - n * y_average * y_average;
  var sxy = sum_xy - n * x_average * y_average;
  var r = sxy*sxy/(sxx*syy);
  return r;
}
 
