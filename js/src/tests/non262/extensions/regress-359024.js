/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 359024;
var summary = 'Do not crash with Script...';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  if (typeof Script == 'undefined')
  {
    print(expect = actual = 'Test skipped. Script object required.');
  }
  else
  {
    var scri=new Script(" var s=new Date(); var a=0; for(var i=0;i<1024*1024;i++) {a=i } var e=new Date(); print('time2='+(e-s)/1000);");
    scri.compile();
    scri.exec();
  }
  reportCompare(expect, actual, summary);
}
