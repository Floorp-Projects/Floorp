/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 365692;
var summary = 'getter/setter bytecodes should support atoms over 64k';
var actual = 'No Crash';
var expect = 'No Crash';


printBugNumber(BUGNUMBER);
printStatus (summary);
 
function g()
{
  return 10;
}

try
{
  var N = 100*1000;
  var src = 'var x = ["';
  var array = Array(N);
  for (var i = 0; i != N; ++i)
    array[i] = i;
  src += array.join('","')+'"]; x.a getter = g; return x.a;';
  var f = Function(src);
  if (f() != 10)
    throw "Unexpected result";
}
catch(ex)
{
  if (ex == "Unexpected result")
  {
    actual = ex;
  }
}
reportCompare(expect, actual, summary);
