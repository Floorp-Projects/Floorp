// A Debugger can't force-return from the first onEnterFrame for an async generator.

ignoreUnhandledRejections();

let g = newGlobal({newCompartment: true});
g.eval(`async function* f(x) { await x; return "ponies"; }`);

let dbg = new Debugger;
dbg.uncaughtExceptionHook = exc => {}; // ignore errors
let gw = dbg.addDebuggee(g);
let hits = 0;
let resumption = undefined;

dbg.onEnterFrame = frame => {
    dbg.onEnterFrame = undefined;
    assertEq(frame.type, "call");
    assertEq(frame.callee.name, "f");
    frame.onPop = completion => {
        hits++;
    };

    // Try to force-return. It's too early. This results in a call to the
    // uncaughtExceptionHook but is otherwise ignored.
    return {return: "rainbows"};
};
let it = g.f(0);    // onPop #1: the initial yield
assertEq(hits, 1);
let p = it.next();  // onPop #2: await x
assertEq(hits, 2);
drainJobQueue();    // onPop #3: return "ponies", #4: the final yield
assertEq(hits, 4);

let pw = gw.makeDebuggeeValue(p);
assertEq(pw.isPromise, true);
assertEq(pw.promiseState, "fulfilled");
assertEq(pw.promiseValue.getProperty("value").return, "ponies");
assertEq(pw.promiseValue.getProperty("done").return, true);

// ----

g.eval(`async function* f2(x) { await x; return "moar ponies"; }`);

let savedAsyncGen = undefined;
dbg.onEnterFrame = frame => {
    dbg.onEnterFrame = undefined;
    assertEq(frame.type, "call");
    assertEq(frame.callee.name, "f2");
    frame.onPop = completion => {
        if (savedAsyncGen === undefined) {
            savedAsyncGen = completion.return;
        }
    };
};
let it2 = g.f2(123);
let p2 = it2.next();
drainJobQueue();

let pw2 = gw.makeDebuggeeValue(p2);
assertEq(pw2.isPromise, true);
assertEq(pw2.promiseState, "fulfilled");
assertEq(pw2.promiseValue.getProperty("value").return, "moar ponies");
assertEq(pw2.promiseValue.getProperty("done").return, true);
