/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Flags: -e "version(170)"

//-----------------------------------------------------------------------------

version(170)

//-----------------------------------------------------------------------------

var du = new Debugger();
if (typeof du.drainTraceLoggerScriptCalls == "function") {
    du.setupTraceLoggerScriptCalls();

    du.startTraceLogger();
    test();
    du.endTraceLogger();

    var objs = du.drainTraceLoggerScriptCalls();
    var scripts = 0;
    var stops = 0;
    for (var i = 0; i < objs.length; i++) {
        if (objs[i].logType == "Script") {
            scripts++;
        } else if (objs[i].logType == "Stop") {
            stops++;
        } else {
            throw "We shouldn't receive non-script events.";
        }
    }
    assertEq(scripts, stops + 1);
    // "+ 1" because we get a start for bug1298541.js:1, but not the stop.
}

function test()
{
  for (var i in (function(){ for (var j=0;j<4;++j) { yield ""; } })());
}
