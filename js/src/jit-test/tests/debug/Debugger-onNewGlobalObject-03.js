// onNewGlobalObject handlers on different Debugger instances are independent.

var dbg1 = new Debugger;
var log1;
function h1(global) {
  log1 += 'n';
  assertEq(global.seen, undefined);
  global.seen = true;
}

var dbg2 = new Debugger;
var log2;
function h2(global) {
  log2 += 'n';
  assertEq(global.seen, undefined);
  global.seen = true;
}

log1 = log2 = '';
newGlobal();
assertEq(log1, '');
assertEq(log2, '');

log1 = log2 = '';
dbg1.onNewGlobalObject = h1;
newGlobal();
assertEq(log1, 'n');
assertEq(log2, '');

log1 = log2 = '';
dbg2.onNewGlobalObject = h2;
newGlobal();
assertEq(log1, 'n');
assertEq(log2, 'n');

log1 = log2 = '';
dbg1.onNewGlobalObject = undefined;
newGlobal();
assertEq(log1, '');
assertEq(log2, 'n');
