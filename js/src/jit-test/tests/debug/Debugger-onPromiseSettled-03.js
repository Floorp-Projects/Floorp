// onPromiseSettled handlers on different Debugger instances are independent.
if (!('Promise' in this))
    quit(0);

var g = newGlobal();
var dbg1 = new Debugger(g);
var log1;
function h1(promise) {
  log1 += 's';
  assertEq(promise.seen, undefined);
  promise.seen = true;
}

var dbg2 = new Debugger(g);
var log2;
function h2(promise) {
  log2 += 's';
  assertEq(promise.seen, undefined);
  promise.seen = true;
}

log1 = log2 = '';
g.settlePromiseNow(new g.Promise(function (){}));
assertEq(log1, '');
assertEq(log2, '');

log1 = log2 = '';
dbg1.onPromiseSettled = h1;
g.settlePromiseNow(new g.Promise(function (){}));
assertEq(log1, 's');
assertEq(log2, '');

log1 = log2 = '';
dbg2.onPromiseSettled = h2;
g.settlePromiseNow(new g.Promise(function (){}));
assertEq(log1, 's');
assertEq(log2, 's');

log1 = log2 = '';
dbg1.onPromiseSettled = undefined;
g.settlePromiseNow(new g.Promise(function (){}));
assertEq(log1, '');
assertEq(log2, 's');
