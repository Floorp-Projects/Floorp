// onPromiseSettled handlers fire, until they are removed.

var g = newGlobal({newCompartment: true});
var dbg = new Debugger(g);
var log;

log = '';
g.settlePromiseNow(new g.Promise(function (){}));
assertEq(log, '');

dbg.onPromiseSettled = function (promise) {
  log += 's';
  assertEq(promise.seen, undefined);
  promise.seen = true;
};

log = '';
g.settlePromiseNow(new g.Promise(function (){}));
assertEq(log, 's');

log = '';
dbg.onPromiseSettled = undefined;
g.settlePromiseNow(new g.Promise(function (){}));
assertEq(log, '');
