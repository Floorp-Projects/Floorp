// |reftest| skip-if(!xulRuntime.shell&&((Android||(isDebugBuild&&xulRuntime.OS=="Linux")||xulRuntime.XPCOMABI.match(/x86_64/)))) slow -- can fail silently due to out of memory, bug 615011 - timeouts on slow debug Linux
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 336409;
var summary = 'Integer overflow in js_obj_toSource';
var actual = 'No Crash';
var expect = /(No Crash|InternalError: allocation size overflow|out of memory)/;

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
  var n = 128;
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
  actual = ex + '';
  print(actual);
}

reportMatch(expect, actual, summary);
