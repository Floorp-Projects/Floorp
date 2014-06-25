/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 327608;
var summary = 'Do not assume we will find the prototype property';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);
print('This test runs only in the browser');
 
function countProps(obj)
{
  var c;
  for (var prop in obj)
    ++c;
  return c;
}

function init()
{
  var inp = document.getElementsByTagName("input")[0];
  countProps(inp);
  gc();
  var blurfun = inp.blur;
  blurfun.__proto__ = null;
  countProps(blurfun);
  reportCompare(expect, actual, summary);
  gDelayTestDriverEnd = false;
  jsTestDriverEnd();
}

if (typeof window != 'undefined')
{
  // delay test driver end
  gDelayTestDriverEnd = true;

  document.write('<input>');
  window.addEventListener("load", init, false);
}
else
{
  reportCompare(expect, actual, summary);
}
