/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 452498;
var summary = 'TM: upvar2 regression tests';
var actual = '';
var expect = '';

//-------  Comment #196  From  Gary Kwong

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

// Assertion failure: localKind == JSLOCAL_VAR || localKind == JSLOCAL_CONST, at ../jsfun.cpp:916

  this.x = undefined;
  this.watch("x", Function);
  NaN = uneval({ get \u3056 (){ return undefined } });
  x+=NaN;

  reportCompare(expect, actual, summary + ': 1');

// Assertion failure: lexdep->isLet(), at ../jsparse.cpp:1900

  (function (){
    var x;
    eval("var x; (function ()x)");
  }
    )();

  reportCompare(expect, actual, summary + ': 2');

  exitFunc ('test');
}
