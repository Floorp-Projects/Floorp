// Debugger.Object.prototype.unwrap should not let us see things in
// invisible-to-Debugger compartments.

load(libdir + 'asserts.js');

var g = newGlobal({ invisibleToDebugger: true });

var dbg = new Debugger;

// Create a wrapper in our compartment for the global.
// Note that makeGlobalObjectReference won't do: it tries to dereference as far
// as it can go.
var /* yo */ DOwg = dbg.makeGlobalObjectReference(this).makeDebuggeeValue(g);

assertThrowsInstanceOf(() => DOwg.unwrap(), Error);
