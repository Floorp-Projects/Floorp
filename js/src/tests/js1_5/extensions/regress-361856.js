/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 361856;
var summary = 'Do not assert: overwriting @ js_AddScopeProperty';
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

  function testit() {
    var obj = {};
    obj.watch("foo", function(){});
    delete obj.foo;
    obj = null;
    gc();
  }
  testit();

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
