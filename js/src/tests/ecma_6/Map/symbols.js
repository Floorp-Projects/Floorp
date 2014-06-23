/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

var m = new Map;

// Symbols can be Map keys.
var sym = Symbol();
m.set(sym, "zero");
assertEq(m.has(sym), true);
assertEq(m.get(sym), "zero");
assertEq(m.has(Symbol()), false);
assertEq(m.get(Symbol()), undefined);
assertEq([...m][0][0], sym);
m.set(sym, "replaced");
assertEq(m.get(sym), "replaced");
m.delete(sym);
assertEq(m.has(sym), false);
assertEq(m.size, 0);

if (typeof reportCompare === "function")
  reportCompare(0, 0);
