// Debugger.Frames for async functions are not GC'd while they're suspended.
// The awaited promise keeps the generator alive, via its reaction lists.

var g = newGlobal({newCompartment: true});
g.eval(`
    // Create a few promises.
    var promises = [], resolvers = [];
    for (let i = 0; i < 3; i++)
        promises.push(new Promise(r => { resolvers.push(r); }));

    async function f() {
        debugger;
        for (let p of promises) {
            await p;
            debugger;
        }
    }
`);
var dbg = new Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    if (hits === 0)
        frame.seen = true;
    else
        assertEq(frame.seen, true);
    hits++;
};

let done = false;
g.f().then(_ => { done = true; });
gc();
drainJobQueue();
gc();

// Resolve the promises one by one.
for (let [i, resolve] of g.resolvers.entries()) {
    assertEq(hits, 1 + i);
    assertEq(done, false);
    resolve("x");
    gc();
    drainJobQueue();
    gc();
}
assertEq(hits, 1 + g.resolvers.length);
assertEq(done, true);
