// Debugger.Object.protoype.getPromiseReactions reports directly resolved promises.

load(libdir + 'array-compare.js');

var g = newGlobal({ newCompartment: true });
var dbg = new Debugger;
var DOg = dbg.addDebuggee(g);

g.eval(`
    var pResolve, pReject;
    var p = new Promise((resolve, reject) => { pResolve = resolve; pReject = reject });
    var p2 = new Promise((resolve, reject) => { resolve(p); });
    var p3 = new Promise((resolve, reject) => { resolve(p); });
    var p4 = new Promise((resolve, reject) => { resolve(p2); });
`);

var [DOp, DOp2, DOp3, DOp4] = [g.p, g.p2, g.p3, g.p4].map(p => DOg.makeDebuggeeValue(p));

// Since the standard resolving functions enqueue a job to do the `then` call,
// none of the reactions should be visible yet.
assertEq(true, arraysEqual(DOp.getPromiseReactions(), []));
assertEq(true, arraysEqual(DOp2.getPromiseReactions(), []));
assertEq(true, arraysEqual(DOp3.getPromiseReactions(), []));
assertEq(true, arraysEqual(DOp4.getPromiseReactions(), []));

// This should let them all appear in place.
drainJobQueue();

assertEq(true, arraysEqual(DOp.getPromiseReactions(), [DOp2, DOp3]));
assertEq(true, arraysEqual(DOp2.getPromiseReactions(), [DOp4]));
assertEq(true, arraysEqual(DOp3.getPromiseReactions(), []));
assertEq(true, arraysEqual(DOp4.getPromiseReactions(), []));

// Resolving the initial promise should kick off its reactions, but propagating
// that the rest of the way requires microtasks.
g.pResolve(42);

assertEq(true, arraysEqual(DOp.getPromiseReactions(), []));
assertEq(true, arraysEqual(DOp2.getPromiseReactions(), [DOp4]));
assertEq(true, arraysEqual(DOp3.getPromiseReactions(), []));
assertEq(true, arraysEqual(DOp4.getPromiseReactions(), []));

// Let the propagation complete.
drainJobQueue();

assertEq(true, arraysEqual(DOp.getPromiseReactions(), []));
assertEq(true, arraysEqual(DOp2.getPromiseReactions(), []));
assertEq(true, arraysEqual(DOp3.getPromiseReactions(), []));
assertEq(true, arraysEqual(DOp4.getPromiseReactions(), []));
