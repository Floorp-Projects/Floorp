// These tests will be using object literals as keys, and we want some of them
// to be dead after being inserted into a WeakMap. That means we must wrap
// everything in functions because it seems like the toplevel script hangs onto
// its object literals.

gczeal(0);
gcparam('minNurseryBytes', 1024 * 1024);
gcparam('maxNurseryBytes', 1024 * 1024);

// All reachable keys should be found, and the rest should be swept.
function basicSweeping() {
  var wm1 = new WeakMap();
  wm1.set({'name': 'obj1'}, {'name': 'val1'});
  var hold = {'name': 'obj2'};
  wm1.set(hold, {'name': 'val2'});
  wm1.set({'name': 'obj3'}, {'name': 'val3'});

  finishgc();
  startgc(100000, 'shrinking');
  gcslice();

  assertEq(wm1.get(hold).name, 'val2');
  assertEq(nondeterministicGetWeakMapKeys(wm1).length, 1);
}

basicSweeping();

// Keep values alive even when they are only referenced by (live) WeakMap values.
function weakGraph() {
  var wm1 = new WeakMap();
  var obj1 = {'name': 'obj1'};
  var obj2 = {'name': 'obj2'};
  var obj3 = {'name': 'obj3'};
  var obj4 = {'name': 'obj4'};
  var clear = {'name': ''}; // Make the interpreter forget about the last obj created

  wm1.set(obj2, obj3);
  wm1.set(obj3, obj1);
  wm1.set(obj4, obj1); // This edge will be cleared
  obj1 = obj3 = obj4 = undefined;

  finishgc();
  startgc(100000, 'shrinking');
  gcslice();

  assertEq(obj2.name, "obj2");
  assertEq(wm1.get(obj2).name, "obj3");
  assertEq(wm1.get(wm1.get(obj2)).name, "obj1");
  print(nondeterministicGetWeakMapKeys(wm1).map(o => o.name).join(","));
  assertEq(nondeterministicGetWeakMapKeys(wm1).length, 2);
}

weakGraph();

// ...but the weakmap itself has to stay alive, too.
function deadWeakMap() {
  var wm1 = new WeakMap();
  var obj1 = makeFinalizeObserver();
  var obj2 = {'name': 'obj2'};
  var obj3 = {'name': 'obj3'};
  var obj4 = {'name': 'obj4'};
  var clear = {'name': ''}; // Make the interpreter forget about the last obj created

  wm1.set(obj2, obj3);
  wm1.set(obj3, obj1);
  wm1.set(obj4, obj1); // This edge will be cleared
  var initialCount = finalizeCount();
  obj1 = obj3 = obj4 = undefined;
  wm1 = undefined;

  finishgc();
  startgc(100000, 'shrinking');
  gcslice();

  assertEq(obj2.name, "obj2");
  assertEq(finalizeCount(), initialCount + 1);
}

deadWeakMap();

// WeakMaps do not strongly reference their keys or values. (WeakMaps hold a
// collection of (strong) references to *edges* from keys to values. If the
// WeakMap is not live, then its edges are of course not live either. An edge
// holds neither its key nor its value live; it just holds a strong ref from
// the key to the value. So if the key is live, the value is live too, but the
// edge itself has no references to anything.)
function deadKeys() {
  var wm1 = new WeakMap();
  var obj1 = makeFinalizeObserver();
  var obj2 = {'name': 'obj2'};
  var obj3 = makeFinalizeObserver();
  var clear = {}; // Make the interpreter forget about the last obj created

  wm1.set(obj1, obj1);
  wm1.set(obj3, obj2);
  obj1 = obj3 = undefined;
  var initialCount = finalizeCount();

  finishgc();
  startgc(100000, 'shrinking');
  gcslice();

  assertEq(finalizeCount(), initialCount + 2);
  assertEq(nondeterministicGetWeakMapKeys(wm1).length, 0);
}

deadKeys();

// The weakKeys table has to grow if it encounters enough new unmarked weakmap
// keys. Trigger this to happen during weakmap marking.
//
// There's some trickiness involved in getting it to test the right thing,
// because if a key is marked before the weakmap, then it won't get entered
// into the weakKeys table. This chains through multiple weakmap layers to
// ensure that the objects can't get marked before the weakmaps.
function weakKeysRealloc() {
  var wm1 = new WeakMap;
  var wm2 = new WeakMap;
  var wm3 = new WeakMap;
  var obj1 = {'name': 'obj1'};
  var obj2 = {'name': 'obj2'};
  wm1.set(obj1, wm2);
  wm2.set(obj2, wm3);
  for (var i = 0; i < 10000; i++) {
    wm3.set(Object.create(null), wm2);
  }
  wm3.set(Object.create(null), makeFinalizeObserver());
  wm2 = undefined;
  wm3 = undefined;
  obj2 = undefined;

  var initialCount = finalizeCount();
  finishgc();
  startgc(100000, 'shrinking');
  gcslice();
  assertEq(finalizeCount(), initialCount + 1);
}

weakKeysRealloc();

// The weakKeys table is populated during regular marking. When a key is later
// deleted, both it and its delegate should be removed from weakKeys.
// Otherwise, it will hold its value live if it gets marked, and our table
// traversals will include non-keys, etc.
function deletedKeys() {
  var wm = new WeakMap;
  var g = newGlobal();

  for (var i = 0; i < 1000; i++)
    wm.set(g.Object.create(null), i);

  finishgc();
  startgc(100, 'shrinking');
  for (var key of nondeterministicGetWeakMapKeys(wm)) {
    if (wm.get(key) % 2)
      wm.delete(key);
  }

  gc();
}

deletedKeys();

// Test adding keys during incremental GC.
function incrementalAdds() {
  var initialCount = finalizeCount();

  var wm1 = new WeakMap;
  var wm2 = new WeakMap;
  var wm3 = new WeakMap;
  var obj1 = {'name': 'obj1'};
  var obj2 = {'name': 'obj2'};
  wm1.set(obj1, wm2);
  wm2.set(obj2, wm3);
  for (var i = 0; i < 10000; i++) {
    wm3.set(Object.create(null), wm2);
  }
  wm3.set(Object.create(null), makeFinalizeObserver());
  obj2 = undefined;

  var obj3 = [];
  finishgc();
  startgc(100, 'shrinking');
  var M = 10;
  var N = 800;
  for (var j = 0; j < M; j++) {
    for (var i = 0; i < N; i++)
      wm3.set(Object.create(null), makeFinalizeObserver()); // Should be swept
    for (var i = 0; i < N; i++) {
      obj3.push({'name': 'obj3'});
      wm1.set(obj3[obj3.length - 1], makeFinalizeObserver()); // Should not be swept
    }
    gcslice();
  }

  wm2 = undefined;
  wm3 = undefined;

  gc();
  print("initialCount = " + initialCount);
  assertEq(finalizeCount(), initialCount + 1 + M * N);
}

incrementalAdds();
