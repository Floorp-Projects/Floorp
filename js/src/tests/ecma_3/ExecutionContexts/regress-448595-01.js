/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 448595;
var summary = 'scope chain var declaration with initialiser in |with| clauses';
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

  var f;

  expect = 'bar';

  f = function(){
    var e = "bar"; 
    with({e:"foo"}) {
      var e = "wibble";
    };

    actual = e;
  }

  f();

  reportCompare(expect, actual, summary + ': with');

  f = function(){
    var e = "bar"; 
    try
    {
      throw {e:"foo"};
    }
    catch(e) {
      var e = "wibble";
    };

    actual = e;
  }

  f();

  reportCompare(expect, actual, summary + ': catch');

  exitFunc ('test');
}
