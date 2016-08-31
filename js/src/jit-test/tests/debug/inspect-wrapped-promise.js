if (typeof Promise === "undefined")
    quit(0);

load(libdir + "asserts.js");

let g = newGlobal();
let dbg = new Debugger();
let gw = dbg.addDebuggee(g);

g.promise1 = new Promise(() => {});
g.promise2 = Promise.resolve(42);
g.promise3 = Promise.reject(42);
g.promise4 = new Object();
g.promise5 = Promise.prototype;

let promiseDO1 = gw.getOwnPropertyDescriptor('promise1').value;
let promiseDO2 = gw.getOwnPropertyDescriptor('promise2').value;
let promiseDO3 = gw.getOwnPropertyDescriptor('promise3').value;
let promiseDO4 = gw.getOwnPropertyDescriptor('promise4').value;
let promiseDO5 = gw.getOwnPropertyDescriptor('promise5').value;

assertEq(promiseDO1.isPromise, true);
assertEq(promiseDO2.isPromise, true);
assertEq(promiseDO3.isPromise, true);
assertEq(promiseDO4.isPromise, false);
assertEq(promiseDO5.isPromise, false);

assertEq(promiseDO1.promiseState, "pending");
assertEq(promiseDO2.promiseState, "fulfilled");
assertEq(promiseDO3.promiseState, "rejected");
assertThrowsInstanceOf(function () { promiseDO4.promiseState }, TypeError);
assertThrowsInstanceOf(function () { promiseDO5.promiseState }, TypeError);

assertEq(promiseDO1.promiseValue, undefined);
assertEq(promiseDO2.promiseValue, 42);
assertEq(promiseDO3.promiseValue, undefined);
assertThrowsInstanceOf(function () { promiseDO4.promiseValue }, TypeError);
assertThrowsInstanceOf(function () { promiseDO5.promiseValue }, TypeError);

assertEq(promiseDO1.promiseReason, undefined);
assertEq(promiseDO2.promiseReason, undefined);
assertEq(promiseDO3.promiseReason, 42);
assertThrowsInstanceOf(function () { promiseDO4.promiseReason }, TypeError);
assertThrowsInstanceOf(function () { promiseDO5.promiseReason }, TypeError);

// Depending on whether async stacks are activated, this can be null, which
// has typeof null.
assertEq(typeof promiseDO1.promiseAllocationSite === "object", true);
assertEq(typeof promiseDO2.promiseAllocationSite === "object", true);
assertEq(typeof promiseDO3.promiseAllocationSite === "object", true);
assertThrowsInstanceOf(function () { promiseDO4.promiseAllocationSite }, TypeError);
assertThrowsInstanceOf(function () { promiseDO5.promiseAllocationSite }, TypeError);

// Depending on whether async stacks are activated, this can be null, which
// has typeof null.
assertThrowsInstanceOf(function () { promiseDO1.promiseResolutionSite }, TypeError);
assertEq(typeof promiseDO2.promiseResolutionSite === "object", true);
assertEq(typeof promiseDO3.promiseResolutionSite === "object", true);
assertThrowsInstanceOf(function () { promiseDO4.promiseResolutionSite }, TypeError);
assertThrowsInstanceOf(function () { promiseDO5.promiseResolutionSite }, TypeError);

assertEq(promiseDO1.promiseID, 1);
assertEq(promiseDO2.promiseID, 2);
assertEq(promiseDO3.promiseID, 3);
assertThrowsInstanceOf(function () { promiseDO4.promiseID }, TypeError);
assertThrowsInstanceOf(function () { promiseDO5.promiseID }, TypeError);

assertEq(typeof promiseDO1.promiseDependentPromises, "object");
assertEq(typeof promiseDO2.promiseDependentPromises, "object");
assertEq(typeof promiseDO3.promiseDependentPromises, "object");
assertThrowsInstanceOf(function () { promiseDO4.promiseDependentPromises }, TypeError);
assertThrowsInstanceOf(function () { promiseDO5.promiseDependentPromises }, TypeError);

assertEq(promiseDO1.promiseDependentPromises.length, 0);
assertEq(promiseDO2.promiseDependentPromises.length, 0);
assertEq(promiseDO3.promiseDependentPromises.length, 0);

assertEq(typeof promiseDO1.promiseLifetime, "number");
assertEq(typeof promiseDO2.promiseLifetime, "number");
assertEq(typeof promiseDO3.promiseLifetime, "number");
assertThrowsInstanceOf(function () { promiseDO4.promiseLifetime }, TypeError);
assertThrowsInstanceOf(function () { promiseDO5.promiseLifetime }, TypeError);

assertThrowsInstanceOf(function () { promiseDO1.promiseTimeToResolution }, TypeError);
assertEq(typeof promiseDO2.promiseTimeToResolution, "number");
assertEq(typeof promiseDO3.promiseTimeToResolution, "number");
assertThrowsInstanceOf(function () { promiseDO4.promiseTimeToResolution }, TypeError);
assertThrowsInstanceOf(function () { promiseDO5.promiseTimeToResolution }, TypeError);
