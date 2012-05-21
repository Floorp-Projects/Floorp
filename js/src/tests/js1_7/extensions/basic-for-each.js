/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER     = "346582";
var summary = "Basic support for iterable objects and for-each";
var actual, expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

var failed = false;

var iterable = { persistedProp: 17 };

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

  // for each sets keysOnly==false
  var index = 0;
  for each (var v in iterable)
  {
    if (!Array_equals(v, [keys[index], vals[index]]))
      throw "for-each iteration failed on index=" + index + "!";
    index++;
  }
  if (index != keys.length)
    throw "not everything iterated!  index=" + index +
      ", keys.length=" + keys.length;

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
