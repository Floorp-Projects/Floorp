// |reftest| skip-if(Android)
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 476869;
var summary = 'Do not assert: v != JSVAL_ERROR_COOKIE';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  if (typeof gczeal == 'undefined')
  {
    gczeal = (function (){});
  }


  function f()
  {
    (new Function("gczeal(1); for (let y of [/x/,'',new Boolean(false),new Boolean(false),new Boolean(false),'',/x/,new Boolean(false),new Boolean(false)]){}"))();
  }
  __proto__.__iterator__ = this.__defineGetter__("", function(){})
    f();


  delete __proto__.__iterator__;

  gczeal(0);

  reportCompare(expect, actual, summary);
}
