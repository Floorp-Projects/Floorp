// |jit-test| error: TestComplete
// onPop can change a throw into a throw of a different value.

load(libdir + "asserts.js");
var g = newGlobal();
var dbg = new Debugger(g);

function test(type, provocation) {
    var log;

    // Help people figure out which 'test' call failed.
    print("type:        " + uneval(type));
    print("provocation: " + uneval(provocation));

    dbg.onDebuggerStatement = function handleDebuggerStatement(f) {
        log += 'd';
    };

    dbg.onEnterFrame = function handleEnterFrame(f) {
        log += '(';
        assertEq(f.type, type);
        f.onPop = function handlePop(c) {
            log += ')';
            assertEq(c.throw, 'mud');
            return { throw: 'snow' };
        };
    };

    log = '';
    assertThrowsValue(provocation, 'snow');
    assertEq(log, "(d)");

    print();
}

g.eval("function f() { debugger; throw 'mud'; }");
test("call", g.f);
test("call", function () { return new g.f; });
test("eval", function () { return g.eval("debugger; throw \'mud\';"); });
test("global", function () { return g.evaluate("debugger; throw \'mud\';"); });
throw 'TestComplete';
