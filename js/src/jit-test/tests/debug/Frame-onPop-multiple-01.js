// Multiple debuggers all get their onPop handlers called.

function completionsEqual(c1, c2) {
    if (c1 && c2) {
        if (c1.throw)
            return c1.throw === c2.throw;
        else
            return c1.return === c2.return;
    }
    return c1 === c2;
}

function completionString(c) {
    if (c == null)
        return 'x';
    if (c.return)
        return 'r' + c.return;
    if (c.throw)
        return 't' + c.throw;
    return '?';
}

var g = newGlobal({newCompartment: true}); // poor thing
g.eval('function f() { debugger; return "1"; }');

// A list of the debuggers' Debugger.Frame instances. When it's all over,
// we test that they are all marked as no longer live.
var frames = [];

// We start off the test via Debugger.Frame.prototype.eval, so if we end
// with a termination, we still catch it, instead of aborting the whole
// test. (Debugger.Object.prototype.executeInGlobal would simplify this...)
var dbg0 = new Debugger(g);
dbg0.onEnterFrame = function handleOriginalEnter(frame) {
    dbg0.log += '(';
    dbg0.onEnterFrame = undefined;

    assertEq(frame.onStack, true);
    frames.push(frame);

    var dbgs = [];
    var log;

    // Create a separate debugger to carry out each item in sequence.
    for (let i = 0; i < 9; i++) {
        // Each debugger's handlers close over a distinct 'dbg', but
        // that's the only distinction between them. Otherwise, they're
        // driven entirely by global data, so the order in which events are
        // dispatched to them shouldn't matter.
        let dbg = new Debugger(g);
        dbgs.push(dbg);

        dbg.onDebuggerStatement = function handleDebuggerStatement(f) {
            log += 'd';  
            assertEq(f.onStack, true);
            frames.push(f);
        };

        // First expect the 'eval'...
        dbg.onEnterFrame = function handleEnterEval(f) {
            log += 'e';
            assertEq(f.type, 'eval');
            assertEq(f.onStack, true);
            frames.push(f);

            // Then expect the call.
            dbg.onEnterFrame = function handleEnterCall(f) {
                log += '(';
                assertEq(f.type, 'call');
                assertEq(f.onStack, true);
                frames.push(f);

                // Don't expect any further frames.
                dbg.onEnterFrame = function handleExtraEnter(f) {
                    log += 'z';
                };

                f.onPop = function handlePop(c) {
                    log += ')';
                    assertEq(this.onStack, true);
                    assertEq(completionsEqual(c, { return: '1' }), true);
                    frames.push(this);

                    // Check that this debugger is in the list, and then remove it.
                    var i = dbgs.indexOf(dbg);
                    assertEq(i != -1, true);
                    dbgs.splice(i,1);
                };
            };
        };
    }

    log = '';
    assertEq(completionsEqual(frame.eval('f()'), { return: '1' }), true);
    assertEq(log, "eeeeeeeee(((((((((ddddddddd)))))))))");

    dbg0.log += '.';
};

dbg0.log = '';
g.eval('eval');
assertEq(dbg0.log, '(.');

// Check that all Debugger.Frame instances we ran into are now marked as dead.
for (var i = 0; i < frames.length; i++)
    assertEq(frames[i].onStack, false);
