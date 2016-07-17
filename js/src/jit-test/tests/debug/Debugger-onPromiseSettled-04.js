// An onPromiseSettled handler can disable itself.
if (!('Promise' in this))
    quit(0);

var g = newGlobal();
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
