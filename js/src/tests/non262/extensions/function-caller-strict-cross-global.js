// |reftest| skip-if(!xulRuntime.shell) -- needs newGlobal()
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var g1 = newGlobal();
g1.evaluate("function f() { return f.caller; }");

var g2 = newGlobal();
g2.f = g1.f;

assertEq(g2.evaluate("function g() { 'use strict'; return f(); } g()"), null);

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
