/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 477758;
var summary = 'TM: RegExp source';
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

  function map(array, func) {
    var result = [];
    for(var i=0;i<array.length;i++) {
      result.push(func(array[i]));
    }
    return result;
  }

  function run() {
    var patterns = [/foo/, /bar/];
    function getSource(r) { return r.source; }
    var patternStrings = map(patterns, getSource);
    print(actual += [patterns[0].source, patternStrings[0]] + '');
  }

  expect = 'foo,foo';

  for (var i = 0; i < 4; i++)
  {
    actual = '';
    run();
    reportCompare(expect, actual, summary + ': ' + i);
  }

  jit(false);

  exitFunc ('test');
}
