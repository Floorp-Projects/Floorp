// Test that the onPromiseSettled hook gets called when a promise settles.

var g = newGlobal();
var dbg = new Debugger();
var gw = dbg.addDebuggee(g);

let log = "";
let pw;
dbg.onPromiseSettled = pw_ => {
  pw = pw_;
  log += "s";
};

let p = g.makeFakePromise();
g.settleFakePromise(p);

assertEq(log, "s");
assertEq(pw, gw.makeDebuggeeValue(p));

log = "";
dbg.enabled = false;
p = g.makeFakePromise();
g.settleFakePromise(p);

assertEq(log, "");
