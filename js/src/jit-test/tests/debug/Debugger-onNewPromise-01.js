// Test that the onNewPromise hook gets called when promises are allocated in
// the scope of debuggee globals.
if (!('Promise' in this))
    quit(0);

var g = newGlobal();
var dbg = new Debugger();
var gw = dbg.addDebuggee(g);


let promisesFound = [];
dbg.onNewPromise = p => { promisesFound.push(p); };

let p1 = new g.Promise(function (){});
dbg.enabled = false;
let p2 = new g.Promise(function (){});

assertEq(promisesFound.indexOf(gw.makeDebuggeeValue(p1)) != -1, true);
assertEq(promisesFound.indexOf(gw.makeDebuggeeValue(p2)) == -1, true);
