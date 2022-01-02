// Globals passed to onNewGlobalObject handers are ready for use immediately.

var dbg = new Debugger;
var log = '';
dbg.onNewGlobalObject = function (global) {
  log += 'n';
  var gw = dbg.addDebuggee(global);
  gw.defineProperty('x', { value: -1 });
  // Check that the global's magic lazy properties are working.
  assertEq(gw.executeInGlobalWithBindings('Math.atan2(y,x)', { y: 0 }).return, Math.PI);
  // Check that the global's prototype is hooked up.
  assertEq(gw.executeInGlobalWithBindings('x.toString()', { x: gw }).return, "[object global]");
};

newGlobal({newCompartment: true});

assertEq(log, 'n');
