/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 349326;
var summary = 'closing generators';
var actual = 'PASS';
var expect = 'PASS';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  let closed;

  function gen()
  {
    try {
      yield 1;
      yield 2;
    } finally {
      closed = true;
    }
  }

// Test that return closes the generator
  function test_return()
  {
    for (let i in gen()) {
      if (i != 1)
        throw "unexpected generator value";
      return 10;
    }
  }

  closed = false;
  test_return();
  if (closed !== true)
    throw "return does not close generator";

// test that break closes generator

  closed = false;
  for (let i in gen()) {
    if (i != 1)
      throw "unexpected generator value";
    break;
  }
  if (closed !== true)
    throw "break does not close generator";

label: {
    for (;;) {
      closed = false;
      for (let i in gen()) {
        if (i != 1)
          throw "unexpected generator value";
        break label;
      }
    }
  }

  if (closed !== true)
    throw "break does not close generator";

// test that an exception closes generator

  function function_that_throws()
  {
    throw function_that_throws;
  }

  try {
    closed = false;
    for (let i in gen()) {
      if (i != 1)
        throw "unexpected generator value";
      function_that_throws();
    }
  } catch (e) {
    if (e !== function_that_throws)
      throw e;
  }

  if (closed !== true)
    throw "exception does not close generator";

// Check consistency of finally execution in presence of generators

  let gen2_was_closed = false;
  let gen3_was_closed = false;
  let finally_was_executed = false;

  function gen2() {
    try {
      yield 2;
    } finally {
      if (gen2_was_closed || !finally_was_executed || !gen3_was_closed)
        throw "bad oder of gen2 finally execution"
          gen2_was_closed = true;
      throw gen2;
    }
  }

  function gen3() {
    try {
      yield 3;
    } finally {
      if (gen2_was_closed || finally_was_executed || gen3_was_closed)
        throw "bad oder of gen3 finally execution"
          gen3_was_closed = true;
    }
  }

label2: {
    try {
      for (let i in gen2()) {
        try {
          for (let j in gen3()) {
            break label2;
          }
        } finally {
          if (gen2_was_closed || finally_was_executed || !gen3_was_closed)
            throw "bad oder of try-finally execution";
          finally_was_executed = true;
        }
      }
      throw "gen2 finally should throw";
    } catch (e) {
      if (e != gen2) throw e;
    }
  }

  print("OK");

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
