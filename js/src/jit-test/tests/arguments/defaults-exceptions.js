load(libdir + "asserts.js");

function die() { throw "x"; }
var ok = true;
function f(a = die()) { ok = false; }
assertThrowsValue(f, "x");
