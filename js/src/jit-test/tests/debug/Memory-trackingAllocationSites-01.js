// Test that we can track allocation sites by setting
// Debugger.Memory.prototype.trackingAllocationSites to true and then get the
// allocation site via Debugger.Object.prototype.allocationSite.

const root = newGlobal();

const dbg = new Debugger();
const wrappedRoot = dbg.addDebuggee(root);

assertEq(dbg.memory.trackingAllocationSites, false);
dbg.memory.trackingAllocationSites = true;
assertEq(dbg.memory.trackingAllocationSites, true);

root.eval("(" + function immediate() {
  this.tests = [
    { name: "object literal",  object: ({}),                  line: Error().lineNumber },
    { name: "array literal",   object: [],                    line: Error().lineNumber },
    { name: "regexp literal",  object: /(two|2)\s*problems/,  line: Error().lineNumber },
    { name: "new constructor", object: new function Ctor(){}, line: Error().lineNumber },
    { name: "new Object",      object: new Object(),          line: Error().lineNumber },
    { name: "new Array",       object: new Array(),           line: Error().lineNumber },
    { name: "new Date",        object: new Date(),            line: Error().lineNumber }
  ];
} + "());");

dbg.memory.trackingAllocationSites = false;
assertEq(dbg.memory.trackingAllocationSites, false);

for (let { name, object, line } of root.tests) {
  print("Entering test: " + name);

  let wrappedObject = wrappedRoot.makeDebuggeeValue(object);
  let allocationSite = wrappedObject.allocationSite;
  print("Allocation site: " + allocationSite);

  assertEq(allocationSite.line, line);
}
