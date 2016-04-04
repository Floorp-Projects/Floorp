if (typeof Promise === "undefined")
    quit(0);

let g = newGlobal();
let dbg = new Debugger();
let gw = dbg.addDebuggee(g);

g.promise = Promise.resolve(42);

let promiseDO = gw.getOwnPropertyDescriptor('promise').value;

assertEq(promiseDO.isPromise, true);

let state = promiseDO.promiseState;
assertEq(state.state, "fulfilled");
assertEq(state.value, 42);
assertEq("reason" in state, true);
assertEq(state.reason, undefined);

let allocationSite = promiseDO.promiseAllocationSite;
// Depending on whether async stacks are activated, this can be null, which
// has typeof null.
assertEq(typeof allocationSite === "object", true);

let resolutionSite = promiseDO.promiseResolutionSite;
// Depending on whether async stacks are activated, this can be null, which
// has typeof null.
assertEq(typeof resolutionSite === "object", true);

assertEq(promiseDO.promiseID, 1);

assertEq(typeof promiseDO.promiseDependentPromises, "object");
assertEq(promiseDO.promiseDependentPromises.length, 0);

assertEq(typeof promiseDO.promiseLifetime, "number");
assertEq(typeof promiseDO.promiseTimeToResolution, "number");
