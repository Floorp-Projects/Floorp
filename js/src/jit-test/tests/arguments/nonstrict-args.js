/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */
load(libdir + 'array-compare.js');

var obj = {};

function args(a) { return arguments; }

var a1, a2, a3, a4;

for (var i = 0; i < 5; i++)
{
  a1 = args();
  a2 = args(1);
  a3 = args(1, obj);
  a4 = args("foopy");
}

assertEq(arraysEqual(a1, []), true);
assertEq(arraysEqual(a2, [1]), true);
assertEq(arraysEqual(a3, [1, obj]), true);
assertEq(arraysEqual(a4, ["foopy"]), true);
