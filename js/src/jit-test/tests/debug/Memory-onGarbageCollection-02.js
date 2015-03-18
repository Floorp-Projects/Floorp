// Test multiple debuggers, GCs, and zones interacting with each other.

gczeal(0);

const root1 = newGlobal();
const dbg1 = new Debugger();
dbg1.addDebuggee(root1)

const root2 = newGlobal();
const dbg2 = new Debugger();
dbg2.addDebuggee(root2)

let fired1 = false;
let fired2 = false;
dbg1.memory.onGarbageCollection = _ => fired1 = true;
dbg2.memory.onGarbageCollection = _ => fired2 = true;

function reset() {
  fired1 = false;
  fired2 = false;
}

// GC 1 only
root1.eval(`gc(this)`);
assertEq(fired1, true);
assertEq(fired2, false);

// GC 2 only
reset();
root2.eval(`gc(this)`);
assertEq(fired1, false);
assertEq(fired2, true);

// Full GC
reset();
gc();
assertEq(fired1, true);
assertEq(fired2, true);

// Full GC with no debuggees
reset();
dbg1.removeAllDebuggees();
dbg2.removeAllDebuggees();
gc();
assertEq(fired1, false);
assertEq(fired2, false);

// One debugger with multiple debuggees in different zones.

dbg1.addDebuggee(root1);
dbg1.addDebuggee(root2);

// Just debuggee 1
reset();
root1.eval(`gc(this)`);
assertEq(fired1, true);
assertEq(fired2, false);

// Just debuggee 2
reset();
root2.eval(`gc(this)`);
assertEq(fired1, true);
assertEq(fired2, false);

// All debuggees
reset();
gc();
assertEq(fired1, true);
assertEq(fired2, false);
