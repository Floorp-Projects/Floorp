// reentering the debugger several times via debuggerHandler and apply/call on a single stack

var g = newGlobal("new-compartment");
var dbg = Debug(g);

function test(usingApply) {
    dbg.hooks = {
        debuggerHandler: function (frame) {
            var n = frame.arguments[0];
            if (n > 1) {
                var result = usingApply ? frame.callee.apply(null, [n - 1])
                                        : frame.callee.call(null, n - 1);
                result.return *= n;
                return result;
            }
        }
    };
    g.eval("function fac(n) { debugger; return 1; }");
    assertEq(g.fac(5), 5 * 4 * 3 * 2 * 1);
}

test(true);
test(false);
