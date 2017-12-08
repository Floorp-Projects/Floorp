// |reftest| skip-if(!xulRuntime.shell||Android) -- bug 524731
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 303213;
var summary = 'integer overflow in js';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);
printStatus('This bug passes if no crash occurs');

expectExitCode(0);
expectExitCode(5);

try
{
  var s=String.fromCharCode(257);

  var ki="";
  var me="";
  for (i = 0; i < 1024; i++)
  {
    ki = ki + s;
  }

  for (i = 0; i < 1024; i++)
  {
    me = me + ki;
  }

  var ov = s;

  for (i = 0; i < 28; i++)
    ov += ov;

  for (i = 0; i < 88; i++)
    ov += me;

  printStatus("done generating");
  var eov = escape(ov);
  printStatus("done escape");
  printStatus(eov);
}
catch(ex)
{
  // handle changed 1.9 branch behavior. see bug 422348
  expect = 'InternalError: allocation size overflow';
  actual = ex + '';
}
 
reportCompare(expect, actual, summary);
