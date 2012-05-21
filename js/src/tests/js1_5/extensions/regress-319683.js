/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 319683;
var summary = 'Do not crash in call_enumerate';
var actual = 'No Crash';
var expect = 'No Crash';
printBugNumber(BUGNUMBER);
printStatus (summary);

function crash(){
  function f(){
    var x;
    function g(){
      x=1; //reference anything here or will not crash.
    }
  }

  //apply an object to the __proto__ attribute
  f.__proto__={};

  //the following call will cause crash
  f();
} 

crash();

reportCompare(expect, actual, summary);
