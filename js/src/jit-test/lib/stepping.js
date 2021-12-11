// Test that stepping through a function stops at the expected lines.
// `script` is a string, some JS code that evaluates to a function.
// `expected` is the array of line numbers where stepping is expected to stop
// when we call the function.
function testStepping(script, expected) {
    let g = newGlobal({newCompartment: true});
    let f = g.eval(script);

    let log = [];
    function maybePause(frame) {
        let previousLine = log[log.length - 1]; // note: may be undefined
        let line = frame.script.getOffsetLocation(frame.offset).lineNumber;
        if (line !== previousLine)
            log.push(line);
    }

    let dbg = new Debugger(g);
    dbg.onEnterFrame = frame => {
        // Log this pause (before the first instruction of the function).
        maybePause(frame);

        // Log future pauses in the same stack frame.
        frame.onStep = function() { maybePause(this); };

        // Now disable this hook so that we step over function calls, not into them.
        dbg.onEnterFrame = undefined;
    };

    f();

    assertEq(log.join(","), expected.join(","));
}
