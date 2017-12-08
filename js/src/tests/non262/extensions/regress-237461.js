/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 237461;
var summary = 'don\'t crash with nested function collides with var';
var actual = 'Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

function g()
{
  var core = {};
  core.js = {};
  core.js.init = function()
    {
      var loader = null;
       
      function loader() {}
    };
  return core;
}

if (typeof Script == 'undefined')
{
  print('Test skipped. Script not defined.');
}
else
{
  var s = new Script(""+g.toString());
  try
  {
    var frozen = s.freeze(); // crash.
    printStatus("len:" + frozen.length);
  }
  catch(e)
  {
  }
}
actual = 'No Crash';

reportCompare(expect, actual, summary);
