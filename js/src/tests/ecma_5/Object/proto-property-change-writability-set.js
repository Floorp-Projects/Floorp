/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributors:
 *   Gary Kwong
 *   Jeff Walden
 *   Jason Orendorff
 */

var accDesc = { set: function() {} };
var dataDesc = { value: 3 };

function f()
{
  constructor = {};
}
function g()
{
  constructor = {};
}

Object.defineProperty(Object.prototype, "constructor", accDesc);
f();
Object.defineProperty(Object.prototype, "constructor", dataDesc);
assertEq(constructor, 3);
f();
assertEq(constructor, 3);
g();
assertEq(constructor, 3);



var a = { p1: 1, p2: 2 };
var b = Object.create(a);
Object.defineProperty(a, "p1", {set: function () {}});
for (var i = 0; i < 2; i++)
{
  b.p1 = {};
  Object.defineProperty(a, "p1", {value: 3});
}
assertEq(b.p1, 3);
assertEq(a.p1, 3);

reportCompare(true, true);
