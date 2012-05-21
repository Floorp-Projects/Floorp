/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 456964;
var summary = 'Infinite loop decompling function';
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
 
  
  function Test()
  {
    var object = { abc: 1, def: 2};
    var o = '';
    for (var i in object)
      o += i + ' = ' + object[i] + '\n';
    return o;
  }

  print(Test);

  expect = 'function Test ( ) { var object = { abc : 1 , def : 2 } ; var o = ""; for ( var i in object ) { o += i + " = " + object [ i ] + "\\ n "; } return o ; }';
  actual = Test + '';

  compareSource(expect, actual, summary);

  exitFunc ('test');
}
