// |reftest| skip-if(xulRuntime.shell||(xulRuntime.OS=="WINNT"&&isDebugBuild)) slow
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 367589;
var summary = 'Do not assert !SPROP_HAS_STUB_SETTER(sprop) || (sprop->attrs & JSPROP_GETTER)';
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
  document.write('<button id="button" onclick="document.getElementsByTagName(\'button\')[0] = \'wtf\';">Crash</button>');
  window.addEventListener('load', crash, false);
}

function crash()
{
  document.getElementById('button').click();
  setTimeout(checkCrash, 0);
}

function checkCrash()
{
  gDelayTestDriverEnd = false;
  reportCompare(expect, actual, summary);
  jsTestDriverEnd();
}
