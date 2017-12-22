// |reftest| skip-if(xulRuntime.shell)
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 443569;
var summary = 'Do not assert: OBJ_IS_NATIVE(obj)';
var actual = 'No Crash';
var expect = 'No Crash';


printBugNumber(BUGNUMBER);
printStatus (summary);

// delay test driver end
gDelayTestDriverEnd = true;

window.addEventListener("load", boom, false);

function boom()
{
  var r = RegExp.prototype;
  r["-1"] = 0;
  Array.prototype.__proto__ = r;
  []["-1"];

  reportCompare(expect, actual, summary);

  gDelayTestDriverEnd = false;
  jsTestDriverEnd();
}


