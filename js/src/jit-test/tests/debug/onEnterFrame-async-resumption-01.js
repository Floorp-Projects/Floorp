// A Debugger can {return:} from the first onEnterFrame for an async function.
// (The exact behavior is undocumented; we're testing that it doesn't crash.)

let g = newGlobal();
g.hit2 = false;
g.eval(`async function f(x) { await x; return "ponies"; }`);

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
        // the robots accept it as one of their own and plug it right into the
        // async function machinery. This may be handy against Skynet someday.
        resumption = frame.eval(`(function* f2() { hit2 = true; throw "fit"; })()`);
        assertEq(resumption.return.class, "Generator");
        return resumption;
    }
};

let p = g.f(0);
assertEq(hits, 1);
let pw = gw.makeDebuggeeValue(p);
assertEq(pw.isPromise, true);
assertEq(pw.promiseState, "rejected");
assertEq(pw.promiseReason, "fit");
