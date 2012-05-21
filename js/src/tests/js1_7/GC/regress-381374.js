// |reftest| skip-if(Android)
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//-----------------------------------------------------------------------------
var BUGNUMBER = 381374;
var summary = 'js_AddScopeProperty - overwrite property with watchpoint';
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
 
  function huh()
  {
    var f;

    Iterator; // ???

    if (0 && 0) {
      eval(""); // ???
    }

    f = new Function("x = 1");

    try {
      f();
    } catch(e) {}
  }

  this.watch('x', function(){});
  this.__defineGetter__('x', new Function());
  huh();
  if (typeof gczeal == 'function')
  {
    gczeal(2); 
  }

  for (y in [0,1]) { this.__defineSetter__('x', function(){}); }

  if (typeof gczeal == 'function')
  {
    gczeal(0);
  }

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
