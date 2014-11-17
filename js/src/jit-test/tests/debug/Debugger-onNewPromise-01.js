// Test that the onNewPromise hook gets called when promises are allocated in
// the scope of debuggee globals.

var g = newGlobal();
var dbg = new Debugger();
var gw = dbg.addDebuggee(g);


let promisesFound = [];
dbg.onNewPromise = p => { promisesFound.push(p); };

let p1 = g.makeFakePromise()
dbg.enabled = false;
let p2 = g.makeFakePromise();

assertEq(promisesFound.indexOf(gw.makeDebuggeeValue(p1)) != -1, true);
assertEq(promisesFound.indexOf(gw.makeDebuggeeValue(p2)) == -1, true);
