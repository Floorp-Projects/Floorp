// Frame properties and methods work in generator-resuming onEnterFrame events.
// Also tests onPop events, for good measure.

let g = newGlobal({newCompartment: true});
g.eval(`\
    function* gen(lo, hi) {
        var a = 1/2;
        yield a;
        yield a * a;
    }
`);
let dbg = new Debugger;
let gw = dbg.addDebuggee(g);

let hits = 0;
let savedScript = null;
let savedEnv = null;
let savedOffsets = new Set;

function check(frame) {
    assertEq(frame.type, "call");
    assertEq(frame.constructing, false);
    assertEq(frame.callee, gw.makeDebuggeeValue(g.gen));

    // `arguments` elements don't work in resumed generator frames,
    // because generators don't keep the arguments around.
    // However, some of this is initialized when the frame.arguments object is
    // created, so if they are created during the first onEnterFrame or onPop
    // event, the properties exist, and those events can also see the values.
    assertEq(frame.arguments.length, args.length);
    for (var i = 0; i < args.length; i++) {
        assertEq(frame.arguments.hasOwnProperty(i), true);

        if (hits < 2)
            assertEq(frame.arguments[i], gw.makeDebuggeeValue(args[i]), `arguments[${i}]`);
        else
            assertEq(frame.arguments[i], undefined);
    }

    if (savedEnv === null) {
        savedEnv = frame.environment;
        assertEq(savedScript, null);
        savedScript = frame.script;
    } else {
        assertEq(frame.environment, savedEnv);
        assertEq(frame.script, savedScript);
    }
    let a_expected = hits < 3 ? undefined : 1/2;
    assertEq(savedEnv.getVariable("a"), a_expected);

    assertEq(frame.generator, true);
    assertEq(frame.live, true);

    let pc = frame.offset;
    assertEq(savedOffsets.has(pc), false);
    savedOffsets.add(pc);

    assertEq(frame.older, null);
    assertEq(frame.this, gw.makeDebuggeeValue(g));
    assertEq(typeof frame.implementation, "string");

    // And the moment of truth:
    assertEq(frame.eval("2 + 2").return, 4);
    assertEq(frame.eval("a").return, a_expected);
    assertEq(frame.eval("if (a !== undefined) { assertEq(a < (lo + hi) / 2, true); } 7;").return, 7);
}

dbg.onEnterFrame = frame => {
    if (frame.type === "eval")
        return;
    check(frame);
    hits++;
    frame.onPop = completion => {
        check(frame);
        hits++;
    };
};

// g.gen ignores the arguments passed to it, but we use them to test
// frame.arguments.
let args = [0, 10, g, dbg];
for (let v of g.gen(...args)) {}
assertEq(hits, 8);
