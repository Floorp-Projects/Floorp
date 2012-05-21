/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 362872;
var summary = 'script should not drop watchpoint that is in use';
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
 
  function exploit() {
    var rooter = {}, object = {}, filler1 = "", filler2 = "\u5555";
    for(var i = 0; i < 32/2-2; i++) { filler1 += "\u5050"; }
    object.watch("foo", function(){
		   object.unwatch("foo");
		   object.unwatch("foo");
		   for(var i = 0; i < 8 * 1024; i++) {
		     rooter[i] = filler1 + filler2;
		   }
		 });
    object.foo = "bar";
  }
  exploit();


  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
