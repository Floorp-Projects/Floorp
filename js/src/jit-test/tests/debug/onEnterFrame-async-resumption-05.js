// A Debugger can {return:} from the first onEnterFrame for an async generator.
// (The exact behavior is undocumented; we're testing that it doesn't crash.)

ignoreUnhandledRejections();

let g = newGlobal({newCompartment: true});
g.hit2 = false;
g.eval(`async function* f(x) { await x; return "ponies"; }`);

let dbg = new Debugger;
let gw = dbg.addDebuggee(g);
let hits = 0;
let resumption = undefined;
dbg.onEnterFrame = frame => {
    if (frame.type == "call" && frame.callee.name === "f") {
        frame.onPop = completion => {
            assertEq(completion.return, resumption.return);
            hits++;
        };

        // Don't tell anyone, but if we force-return a generator object here,
        // the robots will still detect it and throw an error. No protection
        // against Skynet, for us poor humans!
        resumption = frame.eval(`(function* f2() { hit2 = true; })()`);
        assertEq(resumption.return.class, "Generator");
        return resumption;
    }
};

let error;
try {
    g.f(0);
} catch (e) {
    error = e;
}
assertEq(hits, 1);
assertEq(error instanceof g.Error, true);
assertEq(g.hit2, false);
