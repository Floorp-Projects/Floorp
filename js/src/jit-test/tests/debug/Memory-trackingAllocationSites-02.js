// Test that we don't get allocation sites when nobody has asked for them.

const root = newGlobal();

const dbg = new Debugger();
const wrappedRoot = dbg.addDebuggee(root);

dbg.memory.trackingAllocationSites = true;
root.eval("this.obj = {};");
dbg.memory.trackingAllocationSites = false;
root.eval("this.obj2 = {};");

let wrappedObj = wrappedRoot.makeDebuggeeValue(root.obj);
let allocationSite = wrappedObj.allocationSite;
assertEq(allocationSite != null && typeof allocationSite == "object", true);

let wrappedObj2 = wrappedRoot.makeDebuggeeValue(root.obj2);
let allocationSite2 = wrappedObj2.allocationSite;
assertEq(allocationSite2, null);
