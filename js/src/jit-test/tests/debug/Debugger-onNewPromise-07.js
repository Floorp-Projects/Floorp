// Errors in onNewPromise handlers are reported correctly, and don't mess up the
// promise creation.
if (!('Promise' in this))
    quit(0);

var g = newGlobal();
var dbg = new Debugger(g);

let e;
dbg.uncaughtExceptionHook = ee => { e = ee; };
dbg.onNewPromise = () => { throw new Error("woops!"); };

assertEq(typeof new g.Promise(function (){}), "object");
assertEq(!!e, true);
assertEq(!!e.message.match(/woops/), true);
