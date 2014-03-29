assertEq(typeof Debugger.Memory, "function");
let dbg = new Debugger;
assertEq(dbg.memory instanceof Debugger.Memory, true);

load(libdir + "asserts.js");
assertThrowsInstanceOf(() => new Debugger.Memory, TypeError);
