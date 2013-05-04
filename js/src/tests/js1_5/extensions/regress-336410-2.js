// |reftest| skip-if(!xulRuntime.shell&&((isDebugBuild&&xulRuntime.OS=="Linux")||Android||xulRuntime.XPCOMABI.match(/x86_64/)||xulRuntime.OS=="WINNT")) slow -- can fail silently due to out of memory, bug 621348 - timeouts on slow debug Linux
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 336410;
var summary = 'Integer overflow in array_toSource';
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
  printStatus('Creating ' + n + 'M length string');
  var r = createString(n);
  printStatus('Done. length = ' + r.length);
  printStatus('Creating array');
  var o=[r, r, r, r, r, r, r, r, r];
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
