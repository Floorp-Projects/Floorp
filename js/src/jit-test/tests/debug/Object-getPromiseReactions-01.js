// Debugger.Object.prototype.getPromiseReactions throws on non-promises, but
// works on wrappers of promises.

load(libdir + 'asserts.js');
load(libdir + 'array-compare.js');

var g = newGlobal({ newCompartment: true });
var dbg = new Debugger;
var DOg = dbg.addDebuggee(g);

assertThrowsInstanceOf(() => DOg.getPromiseReactions(), TypeError);

// Try retrieving an empty reaction list from an actual promise.
g.eval(`var p = Promise.resolve();`);
var DOgp = DOg.makeDebuggeeValue(g.p);
assertEq(true, arraysEqual(DOgp.getPromiseReactions(), []));

// Try a Debugger.Object of a cross-compartment wrapper of a promise. This
// should still work: the promise accessors generally do checked unwraps of
// their arguments.
var g2 = newGlobal({ newCompartment: true });
DOg2 = dbg.addDebuggee(g2);
var DOg2gp = DOg2.makeDebuggeeValue(g.p);
assertEq(DOgp, DOg2gp.unwrap());
assertEq(true, arraysEqual(DOg2gp.getPromiseReactions(), []));
