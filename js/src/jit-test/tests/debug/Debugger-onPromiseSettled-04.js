// An onPromiseSettled handler can disable itself.
var g = newGlobal({newCompartment: true});
var dbg = new Debugger(g);
var log;

dbg.onPromiseSettled = function (promise) {
  log += 's';
  dbg.onPromiseSettled = undefined;
};

log = '';
g.settlePromiseNow(new g.Promise(function (){}));
g.settlePromiseNow(new g.Promise(function (){}));
assertEq(log, 's');
