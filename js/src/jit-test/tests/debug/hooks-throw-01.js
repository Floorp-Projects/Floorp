// |jit-test| debug
// Basic throw hook test.

load(libdir + "asserts.js");

var g = newGlobal('new-compartment');
var dbg = Debug(g);
var hit = false;
dbg.hooks = {
    throw: function (frame, exc) {
        // hooks.throw is called multiple times as the stack is unwound.
        // Only check the first hit.
        assertEq(arguments.length, 2);
        assertEq(frame instanceof Debug.Frame, true);
        if (!hit) {
            assertEq(exc, 101);
            assertEq(frame.type, "call");
            assertEq(frame.callee.name, "f");
            assertEq(frame.older.type, "eval");
            hit = true;
        }
    }
};

g.eval("function f() { throw 101; }");
assertThrowsValue(function () { g.eval("f();"); }, 101);
assertEq(hit, true);
