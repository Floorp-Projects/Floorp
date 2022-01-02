// Bug 1475669 - Cross-compartment dependent promises.

// Create a promise in realm 1.
let g1 = newGlobal({newCompartment: true});
let p1 = new g1.Promise((_resolve, _reject) => {});

// Add a dependent promise in realm 2.
let g2 = newGlobal({newCompartment: true});
let p2 = g2.Promise.prototype.then.call(p1, g2.eval(`value => {}`));

// Use debugger to find p2 from p1.
let dbg = new Debugger;
let g1w = dbg.addDebuggee(g1);
let g2w = dbg.addDebuggee(g2);
let dependents = g1w.makeDebuggeeValue(p1).promiseDependentPromises;
assertEq(dependents.length, 1);
assertEq(dependents[0], g2w.makeDebuggeeValue(p2));
