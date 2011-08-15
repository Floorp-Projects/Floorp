// tests calling native functions via Debugger.Object.prototype.apply/call

load(libdir + "asserts.js");

var g = newGlobal("new-compartment");
g.eval("function f() { debugger; }");
var dbg = new Debugger(g);

function test(usingApply) {
    dbg.onDebuggerStatement = function (frame) {
        var max = frame.arguments[0];
        var cv = usingApply ? max.apply(null, [9, 16]) : max.call(null, 9, 16);
        assertEq(cv.return, 16);

        cv = usingApply ? max.apply() : max.call();
        assertEq(cv.return, -1/0);

        cv = usingApply ? max.apply(null, [2, 5, 3, 8, 1, 9, 4, 6, 7])
                        : max.call(null, 2, 5, 3, 8, 1, 9, 4, 6, 7);
        assertEq(cv.return, 9);

        // second argument to apply must be an array
        assertThrowsInstanceOf(function () { max.apply(null, 12); }, TypeError);
    };
    g.eval("f(Math.max);");

    dbg.onDebuggerStatement = function (frame) {
        var push = frame.arguments[0];
        var arr = frame.arguments[1];
        var cv;

        cv = usingApply ? push.apply(arr, [0, 1, 2]) : push.call(arr, 0, 1, 2);
        assertEq(cv.return, 3);

        cv = usingApply ? push.apply(arr, [arr]) : push.call(arr, arr);
        assertEq(cv.return, 4);

        cv = usingApply ? push.apply(arr) : push.call(arr);
        assertEq(cv.return, 4);

        // you can apply Array.prototype.push to a string; it does ToObject on it.
        cv = usingApply ? push.apply("hello", ["world"]) : push.call("hello", "world");
        assertEq(cv.return, 6);
    };
    g.eval("var a = []; f(Array.prototype.push, a);");
    assertEq(g.a.length, 4);
    assertEq(g.a.slice(0, 3).join(","), "0,1,2");
    assertEq(g.a[3], g.a);
}

test(true);
test(false);
