// |jit-test| error: TestComplete
// onPop fires when frames are terminated.

var g = newGlobal('new-compartment');
var dbg = new Debugger(g);

// We use Debugger.Frame.prototype.eval and ignore the outer 'eval' frame so we 
// can catch the termination.

function test(type, provocation) {
    // Help people figure out which 'test' call failed.
    print("type:        " + uneval(type));
    print("provocation: " + uneval(provocation));

    var log;
    dbg.onEnterFrame = function handleFirstFrame(f) {
        log += 'f';
        dbg.onDebuggerStatement = function handleDebugger(f) {
            log += 'd';
            return null;
        };

        dbg.onEnterFrame = function handleSecondFrame(f) {
            log += 'e';
            assertEq(f.type, 'eval');

            dbg.onEnterFrame = function handleThirdFrame(f) {
                log += '(';
                assertEq(f.type, type);

                dbg.onEnterFrame = function handleExtraFrames(f) {
                    // This should never be called.
                    assertEq(false, true);
                };

                f.onPop = function handlePop(c) {
                    log += ')';
                    assertEq(c, null);
                };
            };
        };

        assertEq(f.eval(provocation), null);
    };

    log = '';
    // This causes handleFirstFrame to be called.
    assertEq(typeof g.eval('eval'), 'function');
    assertEq(log, 'fe(d)');

    print();
}

g.eval('function f() { debugger; return \'termination fail\'; }');
test('call', 'f();');
test('call', 'new f;');
test('eval', 'eval(\'debugger; \\\'termination fail\\\';\');');
test('global', 'evaluate(\'debugger; \\\'termination fail\\\';\');');
throw 'TestComplete';
