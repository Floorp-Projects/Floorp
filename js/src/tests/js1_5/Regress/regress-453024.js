/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 453024;
var summary = 'Do not assert: vp + 2 + argc <= (jsval *) cx->stackPool.current->avail';
var actual = 'No Crash';
var expect = 'No Crash';

if (typeof window == 'undefined')
{
  reportCompare(true, true, summary + ': test requires browser.');
}
else
{
  gDelayTestDriverEnd = true;
  var j = 0;

  function test()
  {
    enterFunc ('test');
    printBugNumber(BUGNUMBER);
    printStatus (summary);
 
    for (var i = 0; i < 2000; ++i) {
      var ns = document.createElementNS("http://www.w3.org/1999/xhtml", "script");
      var nt = document.createTextNode("++j");
      ns.appendChild(nt);
      document.body.appendChild(ns);
    }

    gDelayTestDriverEnd = false;

    reportCompare(expect, actual, summary);

    jsTestDriverEnd();

    exitFunc ('test');
  }

  window.addEventListener('load', test, false);

}
