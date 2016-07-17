// |jit-test| error: log is not defined

if (!this.Promise)
    throw new Error('log is not defined');

var g = newGlobal();
var dbg = new Debugger(g);
dbg.onPromiseSettled = function (g) { log += 's'; throw "foopy"; };
g.settlePromiseNow(new g.Promise(function (){}));
