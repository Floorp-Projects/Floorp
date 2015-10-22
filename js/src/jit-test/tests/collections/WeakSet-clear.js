var value = {};
var ws = new WeakSet();

assertEq(ws.has(value), false);
// Clearing an already empty WeakSet
assertEq(ws.clear(), undefined);
assertEq(ws.has(value), false);

// Clearing a WeakSet with a live value
ws.add(value);
assertEq(ws.has(value), true);
ws.clear();
assertEq(ws.has(value), false);

// Clearing a WeakSet with values turned to garbage
ws.add(value);
for (var i = 0; i < 10; i++)
    ws.add({});
assertEq(ws.has(value), true);
ws.clear();
assertEq(ws.has(value), false);

// Clearing a WeakSet with values turned to garbage and after doing a GC
ws.add(value);
for (var i = 0; i < 10; i++)
    ws.add({});
assertEq(ws.has(value), true);
gc();
assertEq(ws.has(value), true);
ws.clear();
assertEq(ws.has(value), false);

// More testing when the value is no longer live
ws.add(value);
value = null;
ws.clear();
gc();
var value2 = {};
ws.add(value2);
value2 = null;
gc();
ws.clear();

// Clearing a cross-compartment WeakSet with a live value
ws = new (newGlobal().WeakSet);
value = {};
WeakSet.prototype.add.call(ws, value);
assertEq(WeakSet.prototype.has.call(ws, value), true);
WeakSet.prototype.clear.call(ws);
assertEq(WeakSet.prototype.has.call(ws, value), false);
