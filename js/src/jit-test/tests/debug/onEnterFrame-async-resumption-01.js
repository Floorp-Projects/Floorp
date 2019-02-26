// A Debugger can {return:} from the first onEnterFrame for an async function.
// (The exact behavior is undocumented; we're testing that it doesn't crash.)

ignoreUnhandledRejections();

let g = newGlobal({newCompartment: true});
g.hit2 = false;
g.eval(`async function f(x) { await x; return "ponies"; }`);

let dbg = new Debugger;
let gw = dbg.addDebuggee(g);
let hits = 0;
let resumption = undefined;
dbg.onEnterFrame = frame => {
    if (frame.type == "call" && frame.callee.name === "f") {
        frame.onPop = completion => {
            assertEq(completion.return.isPromise, true);
            hits++;
        };

        // If we force-return a generator object here, the caller will receive
        // a promise object resolved with that generator.
        resumption = frame.eval(`(function* f2() { hit2 = true; })()`);
        assertEq(resumption.return.class, "Generator");
        return resumption;
    }
};

let p = g.f(0);
assertEq(hits, 1);
assertEq(g.hit2, false);
let pw = gw.makeDebuggeeValue(p);
assertEq(pw.isPromise, true);
assertEq(pw.promiseState, "fulfilled");
assertEq(pw.promiseValue, resumption.return);
