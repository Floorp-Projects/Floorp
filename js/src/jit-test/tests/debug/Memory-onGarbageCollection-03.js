// Test that the onGarbageCollection hook is not reentrant.

const root = newGlobal();
const dbg = new Debugger();
const wrappedRoot = dbg.addDebuggee(root)

let timesFired = 0;

dbg.memory.onGarbageCollection = _ => {
  timesFired++;
  root.eval(`gc()`);
}

root.eval(`gc()`);

assertEq(timesFired, 1);
