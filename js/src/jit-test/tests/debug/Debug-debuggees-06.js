// |jit-test| debug
// {has,add,remove}Debuggee throw a TypeError if the argument is invalid.

load(libdir + "asserts.js");

var dbg = new Debug;

function check(val) {
    assertThrowsInstanceOf(function () { dbg.hasDebuggee(val); }, TypeError);
    assertThrowsInstanceOf(function () { dbg.addDebuggee(val); }, TypeError);
    assertThrowsInstanceOf(function () { dbg.removeDebuggee(val); }, TypeError);
}

// Primitive values are invalid.
check(undefined);
check(null);
check(false);
check(1);
check(NaN);
check("ok");

// A Debug.Object that belongs to a different Debug object is invalid.
var g = newGlobal('new-compartment');
var dbg2 = new Debug;
var w = dbg2.addDebuggee(g);
assertEq(w instanceof Debug.Object, true);
check(w);
