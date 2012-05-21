/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 290656;
var summary = 'Regression from bug 254974';
var actual = 'No Error';
var expect = 'No Error';

printBugNumber(BUGNUMBER);
printStatus (summary);

function foo() {
  with(foo) {
    this["insert"] = function(){ var node = new bar(); };
  }
  function bar() {}
}

try
{
  var list = new foo();
  list.insert();
}
catch(e)
{
  actual = e + '';
}

reportCompare(expect, actual, summary);
