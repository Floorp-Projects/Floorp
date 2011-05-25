// Reject non-debug-mode debuggees without asserting.

load(libdir + "asserts.js");

function f() {
    var v = new Debug;
    var g = newGlobal('new-compartment');
    v.addDebuggee(g); // don't assert
}

assertThrowsInstanceOf(f, Error);
