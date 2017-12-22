// |reftest| skip-if(!xulRuntime.shell||Android) slow -- no results reported.
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 336409;
var summary = 'Integer overflow in js_obj_toSource';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

expectExitCode(0);
expectExitCode(5);

function createString(n)
{
  var l = n*1024*1024;
  var r = 'r';

  while (r.length < l)
  {
    r = r + r;
  }
  return r;
}

try
{
  var n = 64;
  printStatus('Creating ' + n + 'MB string');
  var r = createString(n);
  printStatus('Done. length = ' + r.length);
  printStatus('Creating object');
  var o = {f1: r, f2: r, f3: r,f4: r,f5: r, f6: r, f7: r, f8: r,f9: r};
  printStatus('object.toSource()');
  var rr = o.toSource();
  printStatus('Done.');
}
catch(ex)
{
  expect = 'InternalError: allocation size overflow';
  actual = ex + '';
  print(actual);
}

reportCompare(expect, actual, summary);
