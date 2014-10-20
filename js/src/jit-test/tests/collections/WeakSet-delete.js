var ws = new WeakSet;

// Delete on empty
assertEq(ws.delete({}), false);

// Delete existing
var value = {};
ws.add(value);
assertEq(ws.has(value), true);
assertEq(ws.delete(value), true);
assertEq(ws.has(value), false);

// Delete after clear
ws.add(value);
assertEq(ws.has(value), true);
ws.clear();
assertEq(ws.delete(value), false);
assertEq(ws.has(value), false);

// Delete non-empty
for (var i = 0; i < 10; i++)
    ws.add({});
assertEq(ws.add(value), ws);
assertEq(ws.has(value), true);
assertEq(ws.delete(value), true);
assertEq(ws.has(value), false);
assertEq(ws.delete(value), false);
assertEq(ws.has(value), false);

// Delete primitive
assertEq(ws.delete(15), false);
