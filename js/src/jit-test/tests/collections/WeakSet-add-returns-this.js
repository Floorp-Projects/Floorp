var ws = new WeakSet();
var bar = {};
assertEq(ws.add(bar), ws);
var foo = {};
var a = ws.add(foo);
assertEq(a, ws);
assertEq(a.has(bar), true);
assertEq(a.has(foo), true);
assertEq(WeakSet.prototype.add.call(ws, {}), ws);
