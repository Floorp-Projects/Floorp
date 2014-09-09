// |jit-test|

load(libdir + "asm.js");

// Turn on debugging for the current global.
var g = newGlobal();
var dbg = new g.Debugger(this);

assertAsmTypeFail("'use asm'; function f() {} return f");
