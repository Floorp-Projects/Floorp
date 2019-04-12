// A Debugger can't force-return from the first onEnterFrame for an async generator.

ignoreUnhandledRejections();

let g = newGlobal({newCompartment: true});
g.eval(`async function* f(x) { await x; return "ponies"; }`);

let dbg = new Debugger;
let gw = dbg.addDebuggee(g);
let log = "";
let completion = undefined;
let resumption = undefined;
dbg.uncaughtExceptionHook = exc => {
    log += "2";
    assertEq(exc.message, "can't force return from a generator before the initial yield");
    assertEq(exc.constructor, TypeError);
    return undefined;  // Squelch the error and let the debuggee continue.
};
dbg.onEnterFrame = frame => {
    if (frame.type == "call" && frame.callee.name === "f") {
        frame.onPop = c => {
            // We get here after the uncaughtExcpetionHook fires
            // and the debuggee frame has run to the first await.
            completion = c;
            assertEq(completion.return.class, "AsyncGenerator");
            assertEq(completion.return !== resumption.return, true);
            log += "3";
        };

        // Try force-returning an actual object of the expected type.
        dbg.onEnterFrame = undefined;  // don't recurse
        resumption = frame.eval('f(0)');
        assertEq(resumption.return.class, "AsyncGenerator");
        log += "1";
        return resumption;
    }
};

let it = g.f(0);
assertEq(log, "123");
assertEq(gw.makeDebuggeeValue(it), completion.return);
