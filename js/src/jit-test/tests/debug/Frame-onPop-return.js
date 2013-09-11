// |jit-test| error: TestComplete
// onPop fires when frames return normally.

var g = newGlobal();
var dbg = new Debugger(g);

function test(type, provocation) {
    var log;
    var wasConstructing;

    // Help people figure out which 'test' call failed.
    print("type:        " + uneval(type));
    print("provocation: " + uneval(provocation));

    dbg.onDebuggerStatement = function handleDebuggerStatement(f) {
        log += 'd';
    };

    dbg.onEnterFrame = function handleEnterFrame(f) {
        log += '(';
        assertEq(f.type, type);
        wasConstructing = f.constructing;
        f.onPop = function handlePop(c) {
            log += ')';
            assertEq(c.return, 'compliment');
        };
    };

    log = '';
    var result = provocation();
    if (wasConstructing)
        assertEq(typeof result, "object");
    else
        assertEq(result, 'compliment');
    assertEq(log, "(d)");

    print();
}

g.eval("function f() { debugger; return 'compliment'; }");
test("call", g.f);
test("call", function () { return new g.f; });
test("eval", function () { return g.eval("debugger; \'compliment\';"); });
test("global", function () { return g.evaluate("debugger; \'compliment\';"); });
throw 'TestComplete';
