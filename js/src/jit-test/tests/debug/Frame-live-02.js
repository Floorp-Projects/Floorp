// |jit-test| debug
// Debugger.Frame.prototype.live is false for frames that have thrown or been thrown through

load(libdir + "asserts.js");

var g = newGlobal('new-compartment');
g.debuggeeGlobal = this;
g.eval("var finalCheck;");
g.eval("(" + function () {
        var a = [];
        var dbg = Debugger(debuggeeGlobal);
        dbg.onDebuggerStatement = function (frame) {
            a.push(frame);
            for (var i = 0; i < a.length; i++)
                assertEq(a[i].live, true);
        };
        finalCheck = function (n) {
            assertEq(a.length, n);
            for (var i = 0; i < n; i++)
                assertEq(a[i].live, false);
        };
    } + ")()");

function f(n) {
    debugger;
    if (--n > 0)
        f(n);
    else
        throw "fit";
}

assertThrowsValue(function () { f(10); }, "fit");
g.finalCheck(10);
