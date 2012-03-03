// |jit-test| error: TestComplete
// onPop can change a termination into a normal return.

var g = newGlobal('new-compartment');
var dbg = new Debugger(g);

function test(type, provocation) {
    var log;
    var wasConstructing;

    // Help people figure out which 'test' call failed.
    print("type:        " + uneval(type));
    print("provocation: " + uneval(provocation));

    dbg.onDebuggerStatement = function handleDebuggerStatement(f) {
        log += 'd';
        return null;
    };

    dbg.onEnterFrame = function handleEnterFrame(f) {
        log += '(';
        assertEq(f.type, type);
        wasConstructing = f.constructing;
        f.onPop = function handlePop(c) {
            log += ')';
            assertEq(c, null);
            return { return: 'favor' };
        };
    };

    log = '';
    var result = provocation();
    if (wasConstructing)
        assertEq(typeof result, "object");
    else
        assertEq(result, 'favor');
    assertEq(log, "(d)");

    print();
}

g.eval("function f() { debugger; return 'termination fail'; }");
test("call", g.f);
test("call", function () { return new g.f; });
test("eval", function () { return g.eval("debugger; \'termination fail\';"); });
test("global", function () { return g.evaluate("debugger; \'termination fail\';"); });
throw 'TestComplete';
