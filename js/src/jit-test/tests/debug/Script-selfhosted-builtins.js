// The script of self-hosted builtins is not exposed to the debugger and
// instead is reported as |undefined| just like native builtins.

let g = newGlobal({newCompartment: true});

let dbg = new Debugger();
let gw = dbg.addDebuggee(g);

// Array is a known native builtin function.
let nativeBuiltin = gw.makeDebuggeeValue(g.Array);
assertEq(nativeBuiltin.script, undefined);

// Array.prototype[@@iterator] is a known self-hosted builtin function.
let selfhostedBuiltin = gw.makeDebuggeeValue(g.Array.prototype[Symbol.iterator]);
assertEq(selfhostedBuiltin.script, undefined);
