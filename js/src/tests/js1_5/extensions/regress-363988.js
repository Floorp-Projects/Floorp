/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 363988;
var summary = 'Do not crash at JS_GetPrivate with large script';
var actual = 'No Crash';
var expect = 'No Crash';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  function crash() {
    var town = new Array;

    for (var i = 0; i < 0x4001; ++i) {
      var si = String(i);
      town[i] = [ si, "x" + si, "y" + si, "z" + si ];
    }

    return "town=" + uneval(town) + ";function f() {}";
  }

  if (typeof document != "undefined")
  {
    // this is required to reproduce the crash.
    document.write("<script>", crash(), "<\/script>");
  }
  else
  {
    crash();
  }

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
