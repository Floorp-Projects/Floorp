// Test that the onNativeCall hook is called when native function is
// called inside self-hosted JS with Function.prototype.{call,apply}.

load(libdir + 'eqArrayHelper.js');

var g = newGlobal({ newCompartment: true });
var dbg = new Debugger();
var gdbg = dbg.addDebuggee(g);

const rv = [];
dbg.onNativeCall = (callee, reason) => {
  rv.push(callee.name);
};

gdbg.executeInGlobal(`
// Directly call.
dateNow.call();
dateNow.apply();

// Call via bind.
Function.prototype.call.bind(Function.prototype.call)(dateNow);
Function.prototype.apply.bind(Function.prototype.apply)(dateNow);

// Call via std_Function_apply
Reflect.apply(dateNow, null, []);
`);
assertEqArray(rv, [
  "call", "dateNow",
  "apply", "dateNow",
  "bind", "call", "call", "call", "dateNow",
  "bind", "apply", "apply", "apply", "dateNow",
  "apply", "dateNow",
]);
