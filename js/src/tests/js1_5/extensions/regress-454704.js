/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 454704;
var summary = 'Do not crash with defineGetter and XPC wrapper';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  if (typeof XPCSafeJSObjectWrapper != 'undefined' && typeof document != 'undefined')
  {
    gDelayTestDriverEnd = true;
    document.addEventListener('load', boom, true);
  }
  else
  {
    print(expect = actual = 'Test requires browser.');
    reportCompare(expect, actual, summary);
  }
  exitFunc ('test');
}

function boom()
{
  try
  {
    var a = [];
    g = [];
    g.__defineGetter__("f", g.toSource);
    a[0] = g;
    a[1] = XPCSafeJSObjectWrapper(a);
    print("" + a);
  }
  catch(ex)
  {
  }
  gDelayTestDriverEnd = false;
  jsTestDriverEnd();
  reportCompare(expect, actual, summary);
}

