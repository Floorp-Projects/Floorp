/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var out = {};

function arr() {
  return Object.defineProperty([1, 2, 3, 4], 2, {configurable: false});
}

function nonStrict1(out)
{
  var a = out.array = arr();
  a.length = 2;
}

function strict1(out)
{
  "use strict";
  var a = out.array = arr();
  a.length = 2;
  return a;
}

out.array = null;
nonStrict1(out);
assertEq(deepEqual(out.array, [1, 2, 3]), true);

out.array = null;
try
{
  strict1(out);
  throw "no error";
}
catch (e)
{
  assertEq(e instanceof TypeError, true, "expected TypeError, got " + e);
}
assertEq(deepEqual(out.array, [1, 2, 3]), true);

// Internally, SpiderMonkey has two representations for arrays:
// fast-but-inflexible, and slow-but-flexible. Adding a non-index property
// to an array turns it into the latter. We should test on both kinds.
function addx(obj) {
  obj.x = 5;
  return obj;
}

function nonStrict2(out)
{
  var a = out.array = addx(arr());
  a.length = 2;
}

function strict2(out)
{
  "use strict";
  var a = out.array = addx(arr());
  a.length = 2;
}

out.array = null;
nonStrict2(out);
assertEq(deepEqual(out.array, addx([1, 2, 3])), true);

out.array = null;
try
{
  strict2(out);
  throw "no error";
}
catch (e)
{
  assertEq(e instanceof TypeError, true, "expected TypeError, got " + e);
}
assertEq(deepEqual(out.array, addx([1, 2, 3])), true);

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
