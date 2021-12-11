// Debugger.prototype.findAllGlobals surface.

load(libdir + 'asserts.js');

var dbg = new Debugger;
var d = Object.getOwnPropertyDescriptor(Object.getPrototypeOf(dbg), 'findAllGlobals');
assertEq(d.configurable, true);
assertEq(d.enumerable, false);
assertEq(d.writable, true);
assertEq(typeof d.value, 'function');
assertEq(dbg.findAllGlobals.length, 0);
assertEq(dbg.findAllGlobals.name, 'findAllGlobals');

// findAllGlobals can only be applied to real Debugger instances.
assertThrowsInstanceOf(function() {
                         Debugger.prototype.findAllGlobals.call(Debugger.prototype);
                       },
                       TypeError);
var a = dbg.findAllGlobals();
assertEq(a instanceof Array, true);
assertEq(a.length > 0, true);
for (g of a) {
  assertEq(g instanceof Debugger.Object, true);
}
