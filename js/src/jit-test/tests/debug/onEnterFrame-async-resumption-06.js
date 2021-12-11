// A Debugger can {return:} from the first onEnterFrame for an async function.
// (The exact behavior is undocumented; we're testing that it doesn't crash.)

ignoreUnhandledRejections();

let g = newGlobal({newCompartment: true});
g.eval(`async function f(x) { await x; return "ponies"; }`);
g.eval(`async function f2(x) { await x; return "moar ponies"; }`);

let dbg = new Debugger;
let gw = dbg.addDebuggee(g);
let hits = 0;
let resumption = undefined;
let savedAsyncGen = undefined;
dbg.onEnterFrame = frame => {
    if (frame.type == "call" && frame.callee.name === "f2") {
        frame.onPop = completion => {
            if (savedAsyncGen === undefined) {
                savedAsyncGen = completion.return;
            }
        };
    }
    if (frame.type == "call" && frame.callee.name === "f") {
        frame.onPop = completion => {
            hits++;
        };

        return {return: savedAsyncGen};
    }
};

let p2 = g.f2(0);
let p = g.f(0);

assertEq(hits, 1);

drainJobQueue();

assertEq(hits, 1);

let pw2 = gw.makeDebuggeeValue(p2);
assertEq(pw2.isPromise, true);
assertEq(pw2.promiseState, "fulfilled");
assertEq(pw2.promiseValue, "moar ponies");

let pw = gw.makeDebuggeeValue(p);
assertEq(pw.isPromise, true);
assertEq(pw.promiseState, "fulfilled");
assertEq(pw.promiseValue, "moar ponies");
