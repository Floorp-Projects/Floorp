// Test retrieving the log when allocation tracking hasn't ever been enabled.

load(libdir + 'asserts.js');

const root = newGlobal();
const dbg = new Debugger();
dbg.addDebuggee(root)

assertThrowsInstanceOf(() => dbg.memory.drainAllocationsLog(),
                       Error);
