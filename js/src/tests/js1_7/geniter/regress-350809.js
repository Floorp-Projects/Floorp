/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 350809;
var summary = 'Do not assertion: if yield in xml filtering predicate';
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
 
  try
  {
    eval('(function(){ <x/>.(yield 4) })().next();');
  }
  catch(ex)
  {
    actual = expect =
      'InternalError: yield not yet supported from filtering predicate';
  }

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
