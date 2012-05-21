// |reftest| skip-if(!xulRuntime.shell&&(Android||xulRuntime.OS=="WINNT"||xulRuntime.OS=="Linux")) silentfail slow -- bug 528464
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 342960;
var summary = 'Do not crash on large string toSource';
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
 
  expectExitCode(0);
  expectExitCode(5);

  function v()
  {
    var meg="";
    var r="";
    var i;
    print("don't interrupt the script. let it go.");
    for(i=0;i<1024*1024;i++) meg += "v";
    for(i=0;i<1024/8;i++) r += meg;
    var o={f1: r, f2: r, f3: r,f4: r,f5: r, f6: r, f7: r, f8: r,f9: r};
    print('done obj');
    var rr=r.toSource();
    print('done toSource()');
  }

  v();

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
