// |reftest| skip-if(xulRuntime.shell||(xulRuntime.OS=="WINNT"&&isDebugBuild)) slow
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 418540;
var summary = 'Do not assert: OBJ_IS_NATIVE(obj)';
var actual = 'No Crash';
var expect = 'No Crash';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  gDelayTestDriverEnd = true;
  window.onload = boom;
}

function boom()
{
  var p;
  var b = document.createElement("body");
  var v = document.createElement("div");
  b.getAttribute("id")
  v.getAttribute("id")
  for (p in v) { }
  for (p in b) { }
  b.__proto__ = [];
  try { aC(v, null); } catch(e) { }
  try { aC(b, null); } catch(e) { }

  setTimeout(check, 1000);
}

function aC(r, n) { r.appendChild(n); }

function check()
{
  expect = actual = 'No Crash';
  gDelayTestDriverEnd = false;
  reportCompare(expect, actual, summary);
  jsTestDriverEnd();
}
