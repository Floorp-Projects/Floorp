// |reftest| slow
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 465980;
var summary = 'Do not crash @ InitArrayElements';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  function describe(name, startLength, pushArgs, expectThrow, expectLength)
  {
    return name + "(" + startLength + ", " +
      "[" + pushArgs.join(", ") + "], " +
      expectThrow + ", " +
      expectLength + ")";
  }

  var push = Array.prototype.push;
  var unshift = Array.prototype.unshift;

  function testArrayPush(startLength, pushArgs, expectThrow, expectLength)
  {
    print("running testArrayPush(" +
          startLength + ", " +
          "[" + pushArgs.join(", ") + "], " +
          expectThrow + ", " +
          expectLength + ")...");
    var a = new Array(startLength);
    try
    {
      push.apply(a, pushArgs);
      if (expectThrow)
      {
        throw "expected to throw for " +
          describe("testArrayPush", startLength, pushArgs, expectThrow,
                   expectLength);
      }
    }
    catch (e)
    {
      if (!(e instanceof RangeError))
      {
        throw "unexpected exception type thrown: " + e + " for " +
          describe("testArrayPush", startLength, pushArgs, expectThrow,
                   expectLength);
      }
      if (!expectThrow)
      {
        throw "unexpected exception " + e + " for " +
          describe("testArrayPush", startLength, pushArgs, expectThrow,
                   expectLength);
      }
    }

    if (a.length !== expectLength)
    {
      throw "unexpected modified-array length for " +
        describe("testArrayPush", startLength, pushArgs, expectThrow,
                 expectLength);
    }

    for (var i = 0, sz = pushArgs.length; i < sz; i++)
    {
      var index = i + startLength;
      if (a[index] !== pushArgs[i])
      {
        throw "unexpected value " + a[index] +
          " at index " + index + " (" + i + ") during " +
          describe("testArrayPush", startLength, pushArgs, expectThrow,
                   expectLength) + ", expected " + pushArgs[i];
      }
    }
  }

  function testArrayUnshift(startLength, unshiftArgs, expectThrow, expectLength)
  {
    print("running testArrayUnshift(" +
          startLength + ", " +
          "[" + unshiftArgs.join(", ") + "], " +
          expectThrow + ", " +
          expectLength + ")...");
    var a = new Array(startLength);
    try
    {
      unshift.apply(a, unshiftArgs);
      if (expectThrow)
      {
        throw "expected to throw for " +
          describe("testArrayUnshift", startLength, unshiftArgs, expectThrow,
                   expectLength);
      }
    }
    catch (e)
    {
      if (!(e instanceof RangeError))
      {
        throw "unexpected exception type thrown: " + e + " for " +
          describe("testArrayUnshift", startLength, unshiftArgs, expectThrow,
                   expectLength);
      }
      if (!expectThrow)
      {
        throw "unexpected exception " + e + " for " +
          describe("testArrayUnshift", startLength, unshiftArgs, expectThrow,
                   expectLength);
      }
    }

    if (a.length !== expectLength)
    {
      throw "unexpected modified-array length for " +
        describe("testArrayUnshift", startLength, unshiftArgs, expectThrow,
                 expectLength);
    }

    for (var i = 0, sz = unshiftArgs.length; i < sz; i++)
    {
      if (a[i] !== unshiftArgs[i])
      {
        throw "unexpected value at index " + i + " during " +
          describe("testArrayUnshift", startLength, unshiftArgs, expectThrow,
                   expectLength);
      }
    }
  }

  var failed = true;

  try
  {
    var foo = "foo", bar = "bar", baz = "baz";

    testArrayPush(4294967294, [foo], false, 4294967295);
    testArrayPush(4294967294, [foo, bar], true, 4294967295);
    testArrayPush(4294967294, [foo, bar, baz], true, 4294967295);
    testArrayPush(4294967295, [foo], true, 4294967295);
    testArrayPush(4294967295, [foo, bar], true, 4294967295);
    testArrayPush(4294967295, [foo, bar, baz], true, 4294967295);

    testArrayUnshift(4294967294, [foo], false, 4294967295);
    testArrayUnshift(4294967294, [foo, bar], true, 4294967294);
    testArrayUnshift(4294967294, [foo, bar, baz], true, 4294967294);
    testArrayUnshift(4294967295, [foo], true, 4294967295);
    testArrayUnshift(4294967295, [foo, bar], true, 4294967295);
    testArrayUnshift(4294967295, [foo, bar, baz], true, 4294967295);

  }
  catch (e)
  {
    actual = e + '';
  }

  reportCompare(expect, actual, summary);
}
