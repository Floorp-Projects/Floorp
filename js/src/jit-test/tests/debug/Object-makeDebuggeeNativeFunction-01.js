// Debugger.Object.prototype.makeDebuggeeNativeFunction does what it is
// supposed to do.

load(libdir + "asserts.js");

var g = newGlobal({newCompartment: true});
var dbg = new Debugger();
var gw = dbg.addDebuggee(g);

// It would be nice if we could test that this call doesn't produce a CCW,
// and that calling makeDebuggeeValue instead does, but
// Debugger.Object.isProxy only returns true for scripted proxies.
const push = gw.makeDebuggeeNativeFunction(Array.prototype.push);

gw.setProperty("push", push);
g.eval("var x = []; push.call(x, 2); x.push(3)");
assertEq(g.x[0], 2);
assertEq(g.x[1], 3);

// Interpreted functions should throw.
assertThrowsInstanceOf(() => {
  gw.makeDebuggeeNativeFunction(() => {});
}, Error);

// Native functions which have extended slots should throw.
let f;
new Promise(resolve => { f = resolve; })
assertThrowsInstanceOf(() => gw.makeDebuggeeNativeFunction(f), Error);
