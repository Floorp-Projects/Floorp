/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */
load(libdir + 'array-compare.js');

var obj = {};

function noargs() { return arguments; }

var a1, a2, a3;
for (var i = 0; i < 5; i++)
{
  a1 = noargs();
  a2 = noargs(1);
  a3 = noargs(2, obj, 8);
}

assertEq(arraysEqual(a1, []), true);
assertEq(arraysEqual(a2, [1]), true);
assertEq(arraysEqual(a3, [2, obj, 8]), true);
