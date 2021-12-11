// Debugger.prototype.makeGlobalObjectReference should not accept invisible-to-debugger globals.
load(libdir + 'asserts.js');

var g = newGlobal({ newCompartment: true, invisibleToDebugger: true });

assertThrowsInstanceOf(function () {
  (new Debugger).makeGlobalObjectReference(g)
}, TypeError);
