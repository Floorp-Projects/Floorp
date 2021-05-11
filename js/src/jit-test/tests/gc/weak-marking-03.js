// |jit-test| allow-unhandlable-oom

// weakmap marking tests that use the testing mark queue to force an ordering
// of marking.

// We are carefully controlling the sequence of GC events.
gczeal(0);

// If a command-line parameter is given, use it as a substring restriction on
// the tests to run.
var testRestriction = scriptArgs[0];
printErr(`testRestriction is ${testRestriction || '(run all tests)'}`);

function runtest(func) {
  if (testRestriction && ! func.name.includes(testRestriction)) {
    print("\Skipping " + func.name);
  } else {
    print("\nRunning " + func.name);
    func();
  }
}

function reportMarks(prefix = "") {
  const marks = getMarks();
  const current = currentgc();
  const markstr = marks.join("/");
  print(`${prefix}[${current.incrementalState}/${current.sweepGroup}@${current.queuePos}] ${markstr}`);
  return markstr;
}

function startGCMarking() {
  startgc(100000);
  while (gcstate() === "Prepare") {
    gcslice(100000);
  }
}

function purgeKey() {
  const m = new WeakMap();
  const vals = {};
  vals.key = Object.create(null);
  vals.val = Object.create(null);
  m.set(vals.key, vals.val);

  minorgc();

  addMarkObservers([m, vals.key, vals.val]);

  enqueueMark(m);
  enqueueMark("yield");

  enqueueMark(vals.key);
  enqueueMark("yield");

  vals.key = vals.val = null;

  startGCMarking();
  // getMarks() returns map/key/value
  assertEq(getMarks().join("/"), "black/unmarked/unmarked",
           "marked the map black");

  gcslice(100000);
  assertEq(getMarks().join("/"), "black/black/unmarked",
           "key is now marked");

  // Trigger purgeWeakKey: the key is in weakkeys (because it was unmarked when
  // the map was marked), and now we're removing it.
  m.delete(nondeterministicGetWeakMapKeys(m)[0]);

  finishgc(); // Finish the GC
  assertEq(getMarks().join("/"), "black/black/black",
           "at end, value is marked too");

  clearMarkQueue();
  clearMarkObservers();
}

if (this.enqueueMark)
  runtest(purgeKey);

function removeKey() {
  reportMarks("removeKey start: ");

  const m = new WeakMap();
  const vals = {};
  vals.key = Object.create(null);
  vals.val = Object.create(null);
  m.set(vals.key, vals.val);

  minorgc();

  addMarkObservers([m, vals.key, vals.val]);

  enqueueMark(m);
  enqueueMark("yield");

  startGCMarking();
  reportMarks("first: ");
  var marks = getMarks();
  assertEq(marks[0], "black", "map is black");
  assertEq(marks[1], "unmarked", "key not marked yet");
  assertEq(marks[2], "unmarked", "value not marked yet");
  m.delete(vals.key);

  finishgc(); // Finish the GC
  reportMarks("done: ");
  marks = getMarks();
  assertEq(marks[0], "black", "map is black");
  assertEq(marks[1], "black", "key is black");
  assertEq(marks[2], "black", "value is black");

  // Do it again, but this time, remove all other roots.
  m.set(vals.key, vals.val);
  vals.key = vals.val = null;
  startgc(10000);
  while (gcstate() !== "Mark") {
    gcslice(100000);
  }
  marks = getMarks();
  assertEq(marks[0], "black", "map is black");
  assertEq(marks[1], "unmarked", "key not marked yet");
  assertEq(marks[2], "unmarked", "value not marked yet");

  // This was meant to test the weakmap deletion barrier, which would remove
  // the key from weakkeys. Unfortunately, JS-exposed WeakMaps now have a read
  // barrier on lookup that marks the key, and deletion requires a lookup.
  m.delete(nondeterministicGetWeakMapKeys(m)[0]);

  finishgc();
  marks = getMarks();
  assertEq(marks[0], "black", "map is black");
  assertEq(marks[1], "black", "key was blackened by lookup read barrier during deletion");
  assertEq(marks[2], "black", "value is black because map and key are black");

  clearMarkQueue();
  clearMarkObservers();
}

if (this.enqueueMark)
  runtest(removeKey);

// Test:
//   1. mark the map
//     - that inserts the delegate into weakKeys
//   2. nuke the CCW key
//     - removes the delegate from weakKeys
//   3. mark the key
//   4. enter weak marking mode
//
// The problem it's attempting to recreate is that entering weak marking mode
// will no longer mark the value, because there's no delegate to trigger it,
// and the key was not added to weakKeys (because at the time the map was
// scanned, the key had a delegate, so it was added to weakKeys instead.)
function nukeMarking() {
  const g1 = newGlobal({newCompartment: true});

  const vals = {};
  vals.map = new WeakMap();
  vals.key = g1.eval("Object.create(null)");
  vals.val = Object.create(null);
  vals.map.set(vals.key, vals.val);
  vals.val = null;
  gc();

  // Set up the sequence of marking events.
  enqueueMark(vals.map);
  enqueueMark("yield");
  // We will nuke the key's delegate here.
  enqueueMark(vals.key);
  enqueueMark("enter-weak-marking-mode");

  // Okay, run through the GC now.
  startgc(1000000);
  while (gcstate() !== "Mark") {
    gcslice(100000);
  }
  assertEq(gcstate(), "Mark", "expected to yield after marking map");
  // We should have marked the map and then yielded back here.
  nukeCCW(vals.key);
  // Finish up the GC.
  gcslice();

  clearMarkQueue();
}

if (this.enqueueMark)
  runtest(nukeMarking);

function transplantMarking() {
  const g1 = newGlobal({newCompartment: true});

  const vals = {};
  vals.map = new WeakMap();
  let {object, transplant} = transplantableObject();
  vals.key = object;
  object = null;
  vals.val = Object.create(null);
  vals.map.set(vals.key, vals.val);
  vals.val = null;
  gc();

  // Set up the sequence of marking events.
  enqueueMark(vals.map);
  enqueueMark("yield");
  // We will transplant the key here.
  enqueueMark(vals.key);
  enqueueMark("enter-weak-marking-mode");

  // Okay, run through the GC now.
  startgc(1000000);
  while (gcstate() !== "Mark") {
    gcslice(100000);
  }
  assertEq(gcstate(), "Mark", "expected to yield after marking map");
  // We should have marked the map and then yielded back here.
  transplant(g1);
  // Finish up the GC.
  gcslice();

  clearMarkQueue();
}

if (this.enqueueMark)
  runtest(transplantMarking);

// 1. Mark the map
//   => add delegate to weakKeys
// 2. Mark the delegate black
//   => do nothing because we are not in weak marking mode
// 3. Mark the key gray
//   => mark value gray, not that we really care
// 4. Enter weak marking mode
//   => black delegate darkens the key from gray to black
function grayMarkingMapFirst() {
  const g = newGlobal({newCompartment: true});
  const vals = {};
  vals.map = new WeakMap();
  vals.key = g.eval("Object.create(null)");
  vals.val = Object.create(null);
  vals.map.set(vals.key, vals.val);

  g.delegate = vals.key;
  g.eval("dummy = Object.create(null)");
  g.eval("grayRoot().push(delegate, dummy)");
  addMarkObservers([vals.map, vals.key]);
  g.addMarkObservers([vals.key, g.dummy]);
  addMarkObservers([vals.val]);

  gc();

  enqueueMark(vals.map);
  enqueueMark("yield"); // checkpoint 1

  g.enqueueMark(vals.key);
  enqueueMark("yield"); // checkpoint 2

  vals.val = null;
  vals.key = null;
  g.delegate = null;
  g.dummy = null;

  const showmarks = () => {
    print("[map,key,delegate,graydummy,value] marked " + JSON.stringify(getMarks()));
  };

  print("Starting incremental GC");
  startGCMarking();
  // Checkpoint 1, after marking map
  showmarks();
  var marks = getMarks();
  assertEq(marks[0], "black", "map is black");
  assertEq(marks[1], "unmarked", "key is not marked yet");
  assertEq(marks[2], "unmarked", "delegate is not marked yet");

  gcslice(100000);
  // Checkpoint 2, after marking delegate
  showmarks();
  marks = getMarks();
  assertEq(marks[0], "black", "map is black");
  assertEq(marks[1], "unmarked", "key is not marked yet");
  assertEq(marks[2], "black", "delegate is black");

  gcslice();
  // GC complete. Key was marked black (b/c of delegate), then gray marking saw
  // it was already black and skipped it.
  showmarks();
  marks = getMarks();
  assertEq(marks[0], "black", "map is black");
  assertEq(marks[1], "black", "delegate marked key black because of weakmap");
  assertEq(marks[2], "black", "delegate is still black");
  assertEq(marks[3], "gray", "basic gray marking is working");
  assertEq(marks[4], "black", "black map + black delegate => black value");

  clearMarkQueue();
  clearMarkObservers();
  grayRoot().length = 0;
  g.eval("grayRoot().length = 0");
}

if (this.enqueueMark)
  runtest(grayMarkingMapFirst);

function grayMarkingMapLast() {
  const g = newGlobal({newCompartment: true});
  const vals = {};
  vals.map = new WeakMap();
  vals.key = g.eval("Object.create(null)");
  vals.val = Object.create(null);
  vals.map.set(vals.key, vals.val);

  vals.map2 = new WeakMap();
  vals.key2 = g.eval("Object.create(null)");
  vals.val2 = Object.create(null);
  vals.map2.set(vals.key2, vals.val2);

  g.delegate = vals.key;
  g.eval("grayRoot().push(delegate)");
  addMarkObservers([vals.map, vals.key]);
  g.addMarkObservers([vals.key]);
  addMarkObservers([vals.val]);

  grayRoot().push(vals.key2);
  addMarkObservers([vals.map2, vals.key2]);
  g.addMarkObservers([vals.key2]);
  addMarkObservers([vals.val2]);

  const labels = ["map", "key", "delegate", "value", "map2", "key2", "delegate2", "value2"];

  gc();

  g.enqueueMark(vals.key);
  g.enqueueMark(vals.key2);
  enqueueMark("yield"); // checkpoint 1

  vals.val = null;
  vals.key = null;
  g.delegate = null;

  vals.map2 = null; // Important! Second map is never marked, keeps nothing alive.
  vals.key2 = null;
  vals.val2 = null;
  g.delegate2 = null;

  const labeledMarks = () => {
    const info = {};
    const marks = getMarks();
    for (let i = 0; i < labels.length; i++)
      info[labels[i]] = marks[i];
    return info;
  };

  const showmarks = () => {
    print("Marks:");
    for (const [label, mark] of Object.entries(labeledMarks()))
      print(`  ${label}: ${mark}`);
  };

  print("Starting incremental GC");
  startGCMarking();
  // Checkpoint 1, after marking key
  showmarks();
  var marks = labeledMarks();
  assertEq(marks.map, "unmarked", "map is unmarked");
  assertEq(marks.key, "unmarked", "key is not marked yet");
  assertEq(marks.delegate, "black", "delegate is black");
  assertEq(marks.map2, "unmarked", "map2 is unmarked");
  assertEq(marks.key2, "unmarked", "key2 is not marked yet");
  assertEq(marks.delegate2, "black", "delegate2 is black");

  gcslice();
  // GC complete. When entering weak marking mode, black delegate propagated to
  // key.
  showmarks();
  marks = labeledMarks();
  assertEq(marks.map, "black", "map is black");
  assertEq(marks.key, "black", "delegate marked key black because of weakmap");
  assertEq(marks.delegate, "black", "delegate is still black");
  assertEq(marks.value, "black", "black map + black delegate => black value");
  assertEq(marks.map2, "dead", "map2 is dead");
  assertEq(marks.key2, "gray", "key2 marked gray, map2 had no effect");
  assertEq(marks.delegate2, "black", "delegate artificially marked black via mark queue");
  assertEq(marks.value2, "dead", "dead map + black delegate => dead value");

  clearMarkQueue();
  clearMarkObservers();
  grayRoot().length = 0;
  g.eval("grayRoot().length = 0");

  return vals; // To prevent the JIT from optimizing out vals.
}

if (this.enqueueMark)
  runtest(grayMarkingMapLast);

function grayMapKey() {
  const vals = {};
  vals.m = new WeakMap();
  vals.key = Object.create(null);
  vals.val = Object.create(null);
  vals.m.set(vals.key, vals.val);

  // Maps are allocated black, so we won't be able to mark it gray during the
  // first GC.
  gc();

  addMarkObservers([vals.m, vals.key, vals.val]);

  // Wait until we can mark gray (ie, sweeping). Mark the map gray and yield.
  // This should happen all in one slice.
  enqueueMark("set-color-gray");
  enqueueMark(vals.m);
  enqueueMark("unset-color");
  enqueueMark("yield");

  // Make the weakmap no longer reachable from the roots, so we can mark it
  // gray.
  vals.m = null;

  enqueueMark(vals.key);
  enqueueMark("yield");

  vals.key = vals.val = null;

  startGCMarking();
  assertEq(getMarks().join("/"), "gray/unmarked/unmarked",
           "marked the map gray");

  gcslice(100000);
  assertEq(getMarks().join("/"), "gray/black/unmarked",
           "key is now marked black");

  finishgc(); // Finish the GC

  assertEq(getMarks().join("/"), "gray/black/gray",
           "at end: black/gray => gray");

  clearMarkQueue();
  clearMarkObservers();
}

if (this.enqueueMark)
  runtest(grayMapKey);

function grayKeyMap() {
  const vals = {};
  vals.m = new WeakMap();
  vals.key = Object.create(null);
  vals.val = Object.create(null);
  vals.m.set(vals.key, vals.val);

  addMarkObservers([vals.m, vals.key, vals.val]);

  enqueueMark(vals.key);
  enqueueMark("yield");

  // Wait until we are gray marking.
  enqueueMark("set-color-gray");
  enqueueMark(vals.m);
  enqueueMark("unset-color");
  enqueueMark("yield");

  enqueueMark("set-color-black");
  enqueueMark(vals.m);
  enqueueMark("unset-color");

  // Make the weakmap no longer reachable from the roots, so we can mark it
  // gray.
  vals.m = null;

  vals.key = vals.val = null;

  // Only mark this zone, to avoid interference from other tests that may have
  // created additional zones.
  schedulezone(vals);

  startGCMarking();
  // getMarks() returns map/key/value
  reportMarks("1: ");
  assertEq(getMarks().join("/"), "unmarked/black/unmarked",
           "marked key black");

  // We always yield before sweeping (in the absence of zeal), so we will see
  // the unmarked state another time.
  gcslice(100000);
  reportMarks("2: ");
  assertEq(getMarks().join("/"), "unmarked/black/unmarked",
           "marked key black, yield before sweeping");

  gcslice(100000);
  reportMarks("3: ");
  assertEq(getMarks().join("/"), "gray/black/gray",
           "marked the map gray, which marked the value when map scanned");

  finishgc(); // Finish the GC
  reportMarks("4: ");
  assertEq(getMarks().join("/"), "black/black/black",
           "further marked the map black, so value should also be blackened");

  clearMarkQueue();
  clearMarkObservers();
}

if (this.enqueueMark)
  runtest(grayKeyMap);

// Cause a key to be marked black *during gray marking*, by first marking a
// delegate black, then marking the map and key gray. When the key is scanned,
// it should be seen to be a CCW of a black delegate and so should itself be
// marked black.
//
// The bad behavior being prevented is:
//
//  1. You wrap an object in a CCW and use it as a weakmap key to some
//     information.
//  2. You keep a strong reference to the object (in its compartment).
//  3. The only references to the CCW are gray, and are in fact part of a cycle.
//  4. The CC runs and discards the CCW.
//  5. You look up the object in the weakmap again. This creates a new wrapper
//     to use as a key. It is not in the weakmap, so the information you stored
//     before is not found. (It may have even been collected, if you had no
//     other references to it.)
//
function blackDuringGray() {
  const g = newGlobal({newCompartment: true});
  const vals = {};
  vals.map = new WeakMap();
  vals.key = g.eval("Object.create(null)");
  vals.val = Object.create(null);
  vals.map.set(vals.key, vals.val);

  g.delegate = vals.key;
  addMarkObservers([vals.map, vals.key]);
  g.addMarkObservers([vals.key]);
  addMarkObservers([vals.val]);
  // Mark observers: map, key, delegate, value

  gc();

  g.enqueueMark(vals.key); // Mark the delegate black
  enqueueMark("yield"); // checkpoint 1

  // Mark the map gray. This will scan through all entries, find our key, and
  // mark it black because its delegate is black.
  enqueueMark("set-color-gray");
  enqueueMark(vals.map); // Mark the map gray

  vals.map = null;
  vals.val = null;
  vals.key = null;
  g.delegate = null;

  const showmarks = () => {
    print("[map,key,delegate,value] marked " + JSON.stringify(getMarks()));
  };

  print("Starting incremental GC");
  startGCMarking();
  // Checkpoint 1, after marking delegate black
  showmarks();
  var marks = getMarks();
  assertEq(marks[0], "unmarked", "map is not marked yet");
  assertEq(marks[1], "unmarked", "key is not marked yet");
  assertEq(marks[2], "black", "delegate is black");
  assertEq(marks[3], "unmarked", "values is not marked yet");

  finishgc();
  showmarks();
  marks = getMarks();
  assertEq(marks[0], "gray", "map is gray");
  assertEq(marks[1], "black", "black delegate should mark key black");
  assertEq(marks[2], "black", "delegate is still black");
  assertEq(marks[3], "gray", "gray map + black key => gray value");

  clearMarkQueue();
  clearMarkObservers();
  grayRoot().length = 0;
  g.eval("grayRoot().length = 0");
}

if (this.enqueueMark)
  runtest(blackDuringGray);

// Same as above, except relying on the implicit edge from delegate -> key.
function blackDuringGrayImplicit() {
  const g = newGlobal({newCompartment: true});
  const vals = {};
  vals.map = new WeakMap();
  vals.key = g.eval("Object.create(null)");
  vals.val = Object.create(null);
  vals.map.set(vals.key, vals.val);

  g.delegate = vals.key;
  addMarkObservers([vals.map, vals.key]);
  g.addMarkObservers([vals.key]);
  addMarkObservers([vals.val]);
  // Mark observers: map, key, delegate, value

  gc();

  // Mark the map gray. This will scan through all entries, find our key, and
  // add implicit edges from delegate -> key and delegate -> value.
  enqueueMark("set-color-gray");
  enqueueMark(vals.map); // Mark the map gray
  enqueueMark("yield"); // checkpoint 1

  enqueueMark("set-color-black");
  g.enqueueMark(vals.key); // Mark the delegate black, propagating to key.

  vals.map = null;
  vals.val = null;
  vals.key = null;
  g.delegate = null;

  const showmarks = () => {
    print("[map,key,delegate,value] marked " + JSON.stringify(getMarks()));
  };

  print("Starting incremental GC");
  startGCMarking();
  // Checkpoint 1, after marking map gray
  showmarks();
  var marks = getMarks();
  assertEq(marks[0], "gray", "map is gray");
  assertEq(marks[1], "unmarked", "key is not marked yet");
  assertEq(marks[2], "unmarked", "delegate is not marked yet");
  assertEq(marks[3], "unmarked", "value is not marked yet");

  finishgc();
  showmarks();
  marks = getMarks();
  assertEq(marks[0], "gray", "map is gray");
  assertEq(marks[1], "black", "black delegate should mark key black");
  assertEq(marks[2], "black", "delegate is black");
  assertEq(marks[3], "gray", "gray map + black key => gray value via delegate -> value");

  clearMarkQueue();
  clearMarkObservers();
  grayRoot().length = 0;
  g.eval("grayRoot().length = 0");
}

if (this.enqueueMark)
  runtest(blackDuringGrayImplicit);
