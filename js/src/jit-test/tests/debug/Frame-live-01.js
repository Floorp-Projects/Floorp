// |jit-test| debug
// Debugger.Frame.prototype.live is true for frames on the stack and false for
// frames that have returned

var desc = Object.getOwnPropertyDescriptor(Debugger.Frame.prototype, "live");
assertEq(typeof desc.get, "function");
assertEq(desc.set, undefined);
assertEq(desc.configurable, true);
assertEq(desc.enumerable, false);

var loc;

var g = newGlobal('new-compartment');
g.debuggeeGlobal = this;
g.eval("var hits = 0;");
g.eval("(" + function () {
        var a = [];
        var dbg = Debugger(debuggeeGlobal);
        dbg.onDebuggerStatement = function (frame) {
            var loc = debuggeeGlobal.loc;
            a[loc] = frame;
            for (var i = 0; i < a.length; i++) {
                assertEq(a[i] === frame, i === loc);
                assertEq(!!(a[i] && a[i].live), i >= loc);
            }
            hits++;
        };
    } + ")()");

function f(n) {
    loc = n; debugger;
    if (n !== 0) {
        f(n - 1);
        loc = n; debugger;
        eval("f(n - 1);");
        loc = n; debugger;
    }
}

f(4);
assertEq(g.hits, 16 + 8*3 + 4*3 + 2*3 + 1*3);

