/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 310607;
var summary = 'Do not crash iterating over Object.prototype';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

var f = new Foo();
f.go("bar");

function Foo() {
  this.go = function(prototype) {
    printStatus("Start");
    for(var i in Object.prototype) {
      printStatus("Here");
      eval("5+4");
    }
    printStatus("End");
  };
}
 
reportCompare(expect, actual, summary);
