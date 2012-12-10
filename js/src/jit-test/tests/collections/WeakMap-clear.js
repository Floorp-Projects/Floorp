var key = {};
var wm = WeakMap();

assertEq(wm.has(key), false);
// Clearing an already empty WeakMap
wm.clear();
assertEq(wm.has(key), false);

// Clearing a WeakMap with a live key
wm.set(key, 42);
assertEq(wm.has(key), true);
wm.clear();
assertEq(wm.has(key), false);

// Clearing a WeakMap with keys turned to garbage
wm.set(key, {});
for (var i = 0; i < 10; i++)
    wm.set({}, {});
assertEq(wm.has(key), true);
wm.clear();
assertEq(wm.has(key), false);

// Clearing a WeakMap with keys turned to garbage and after doing a GC
wm.set(key, {});
for (var i = 0; i < 10; i++)
    wm.set({}, {});
assertEq(wm.has(key), true);
gc();
assertEq(wm.has(key), true);
wm.clear();
assertEq(wm.has(key), false);

// More testing when the key is no longer live
wm.set(key, {});
key = null;
wm.clear();
gc();
var key2 = {};
wm.set(key2, {});
key2 = null;
gc();
wm.clear();
