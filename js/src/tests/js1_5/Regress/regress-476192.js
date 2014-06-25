/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 476192;
var summary = 'TM: Do not assert: JSVAL_TAG(v) == JSVAL_STRING';
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

  jit(true);

  var global;

  (function(){
    var ad = {present: ""};
    var params = ['present', 'a', 'present', 'a', 'present', 'a', 'present'];
    for (var j = 0; j < params.length; j++) {
      global = ad[params[j]];
    }
  })();

  jit(false);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
