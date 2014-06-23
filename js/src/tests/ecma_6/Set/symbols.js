/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

var s = new Set;

// Symbols can be stored in Sets.
var sym = Symbol();
s.add(sym);
assertEq(s.has(sym), true);
assertEq(s.has(Symbol()), false);
assertEq([...s][0], sym);
s.add(sym);
assertEq(s.has(sym), true);
assertEq(s.size, 1);
s.delete(sym);
assertEq(s.has(sym), false);
assertEq(s.size, 0);

if (typeof reportCompare === "function")
  reportCompare(0, 0);
