// |reftest| skip-if(xulRuntime.shell||(xulRuntime.OS=="WINNT"&&isDebugBuild)) slow
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 425360;
var summary = 'Do not assert: !cx->throwing';
var actual = 'No Crash';
var expect = 'No Crash';

function finishtest()
{
  gDelayTestDriverEnd = false;
  reportCompare(expect, actual, summary);
  jsTestDriverEnd();
}

function throwBlah()
{
  throw 'blah';
}

printBugNumber(BUGNUMBER);
printStatus (summary);

gDelayTestDriverEnd = true;
window.onerror = null;
setTimeout('finishtest()', 1000);
window.onload = (function () { setInterval('throwBlah()', 0); });
setInterval('foo(', 0);
