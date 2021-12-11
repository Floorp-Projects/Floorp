// Test that the onNewPromise hook gets called when promises are allocated in
// the scope of debuggee globals.

var g = newGlobal({newCompartment: true});
var dbg = new Debugger();
var gw = dbg.addDebuggee(g);


let promisesFound = [];
dbg.onNewPromise = p => { promisesFound.push(p); };

let p1 = new g.Promise(function (){});
assertEq(promisesFound.indexOf(gw.makeDebuggeeValue(p1)) != -1, true);
