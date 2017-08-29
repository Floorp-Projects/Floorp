/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER     = "(none)";
var summary = "Basic support for accessing iterable objects using Iterator";
var actual, expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

var failed = false;

function Array_equals(a, b)
{
  if (!(a instanceof Array) || !(b instanceof Array))
    throw new Error("Arguments not both of type Array");
  if (a.length != b.length)
    return false;
  for (var i = 0, sz = a.length; i < sz; i++)
    if (a[i] !== b[i])
      return false;
  return true;
}

var iterable = { persistedProp: 17 };

try
{
  // nothing unusual so far -- verify basic properties
  for (var i in iterable)
  {
    if (i != "persistedProp")
      throw "no persistedProp!";
    if (iterable[i] != 17)
      throw "iterable[\"persistedProp\"] == 17";
  }

  var keys = ["foo", "bar", "baz"];
  var vals = [6, 5, 14];

  iterable.__iterator__ =
    function(keysOnly)
    {
      var gen =
      function()
      {
	for (var i = 0; i < keys.length; i++)
	{
	  if (keysOnly)
	    yield keys[i];
	  else
	    yield [keys[i], vals[i]];
	}
      };
      return gen();
    };

  /* Test [key, value] Iterator */
  var index = 0;
  var lastSeen = "INITIALVALUE";
  var it = Iterator(iterable);
  try
  {
    while (true)
    {
      var nextVal = it.next();
      if (!Array_equals(nextVal, [keys[index], vals[index]]))
        throw "Iterator(iterable): wrong next result\n" +
	  "  expected: " + [keys[index], vals[index]] + "\n" +
	  "  actual:   " + nextVal;
      lastSeen = keys[index];
      index++;
    }
  }
  catch (e)
  {
    if (lastSeen !== keys[keys.length - 1])
      throw "Iterator(iterable): not everything was iterated!\n" +
	"  last iterated was: " + lastSeen + "\n" +
	"  error: " + e;
    if (e !== StopIteration)
      throw "Iterator(iterable): missing or invalid StopIteration";
  }

  if (iterable.persistedProp != 17)
    throw "iterable.persistedProp not persisted!";

  /* Test [key, value] Iterator, called with an explicit |false| parameter */
  var index = 0;
  lastSeen = "INITIALVALUE";
  it = Iterator(iterable, false);
  try
  {
    while (true)
    {
      var nextVal = it.next();
      if (!Array_equals(nextVal, [keys[index], vals[index]]))
        throw "Iterator(iterable, false): wrong next result\n" +
	  "  expected: " + [keys[index], vals[index]] + "\n" +
	  "  actual:   " + nextVal;
      lastSeen = keys[index];
      index++;
    }
  }
  catch (e)
  {
    if (lastSeen !== keys[keys.length - 1])
      throw "Iterator(iterable, false): not everything was iterated!\n" +
	"  last iterated was: " + lastSeen + "\n" +
	"  error: " + e;
    if (e !== StopIteration)
      throw "Iterator(iterable, false): missing or invalid StopIteration";
  }

  if (iterable.persistedProp != 17)
    throw "iterable.persistedProp not persisted!";

  /* Test key-only Iterator */
  index = 0;
  lastSeen = undefined;
  it = Iterator(iterable, true);
  try
  {
    while (true)
    {
      var nextVal = it.next();
      if (nextVal !== keys[index])
        throw "Iterator(iterable, true): wrong next result\n" +
	  "  expected: " + keys[index] + "\n" +
	  "  actual:   " + nextVal;
      lastSeen = keys[index];
      index++;
    }
  }
  catch (e)
  {
    if (lastSeen !== keys[keys.length - 1])
      throw "Iterator(iterable, true): not everything was iterated!\n" +
	"  last iterated was: " + lastSeen + "\n" +
	"  error: " + e;
    if (e !== StopIteration)
      throw "Iterator(iterable, true): missing or invalid StopIteration";
  }

  if (iterable.persistedProp != 17)
    throw "iterable.persistedProp not persisted!";
}
catch (e)
{
  failed = e;
}



expect = false;
actual = failed;

reportCompare(expect, actual, summary);
