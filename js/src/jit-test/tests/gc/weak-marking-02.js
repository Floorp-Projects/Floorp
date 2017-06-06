// These tests will be using object literals as keys, and we want some of them
// to be dead after being inserted into a WeakMap. That means we must wrap
// everything in functions because it seems like the toplevel script hangs onto
// its object literals.

// Cross-compartment WeakMap keys work by storing a cross-compartment wrapper
// in the WeakMap, and the actual "delegate" object in the target compartment
// is the thing whose liveness is checked.

gczeal(0);

var g2 = newGlobal();
g2.eval('function genObj(name) { return {"name": name} }');

function basicSweeping() {
  var wm1 = new WeakMap();
  wm1.set({'name': 'obj1'}, {'name': 'val1'});
  var hold = g2.genObj('obj2');
  wm1.set(hold, {'name': 'val2'});
  wm1.set({'name': 'obj3'}, {'name': 'val3'});
  var obj4 = g2.genObj('obj4');
  wm1.set(obj4, {'name': 'val3'});
  obj4 = undefined;

  startgc(100000, 'shrinking');
  gcslice();
  assertEq(wm1.get(hold).name, 'val2');
  assertEq(nondeterministicGetWeakMapKeys(wm1).length, 1);
}

basicSweeping();

// Same, but behind an additional WM layer, to avoid ordering problems (not
// that I've checked that basicSweeping even has any problems.)

function basicSweeping2() {
  var wm1 = new WeakMap();
  wm1.set({'name': 'obj1'}, {'name': 'val1'});
  var hold = g2.genObj('obj2');
  wm1.set(hold, {'name': 'val2'});
  wm1.set({'name': 'obj3'}, {'name': 'val3'});
  var obj4 = g2.genObj('obj4');
  wm1.set(obj4, {'name': 'val3'});
  obj4 = undefined;

  var base1 = {'name': 'base1'};
  var base2 = {'name': 'base2'};
  var wm_base1 = new WeakMap();
  var wm_base2 = new WeakMap();
  wm_base1.set(base1, wm_base2);
  wm_base2.set(base2, wm1);
  wm1 = wm_base2 = undefined;

  startgc(100000, 'shrinking');
  gcslice();

  assertEq(nondeterministicGetWeakMapKeys(wm_base1).length, 1);
  wm_base2 = wm_base1.get(base1);
  assertEq(nondeterministicGetWeakMapKeys(wm_base2).length, 1);
  assertEq(nondeterministicGetWeakMapKeys(wm_base1)[0], base1);
  assertEq(nondeterministicGetWeakMapKeys(wm_base2)[0], base2);
  wm_base2 = wm_base1.get(base1);
  wm1 = wm_base2.get(base2);
  assertEq(wm1.get(hold).name, 'val2');
  assertEq(nondeterministicGetWeakMapKeys(wm1).length, 1);
}

basicSweeping2();

// Scatter the weakmap, the keys, and the values among different compartments.

function tripleZoneMarking() {
  var g1 = newGlobal();
  var g2 = newGlobal();
  var g3 = newGlobal();

  var wm = g1.eval("new WeakMap()");
  var key = g2.eval("({'name': 'obj1'})");
  var value = g3.eval("({'name': 'val1'})");
  g1 = g2 = g3 = undefined;
  wm.set(key, value);

  // Make all of it only reachable via a weakmap in the main test compartment,
  // so that all of this happens during weak marking mode. Use the weakmap as
  // its own key, so we know that the weakmap will get traced before the key
  // and therefore will populate the weakKeys table and all of that jazz.
  var base_wm = new WeakMap();
  base_wm.set(base_wm, [ wm, key ]);

  wm = key = value = undefined;

  startgc(100000, 'shrinking');
  gcslice();

  var keys = nondeterministicGetWeakMapKeys(base_wm);
  assertEq(keys.length, 1);
  var [ wm, key ] = base_wm.get(keys[0]);
  assertEq(key.name, "obj1");
  value = wm.get(key);
  assertEq(value.name, "val1");
}

tripleZoneMarking();

function enbugger() {
  var g = newGlobal();
  var dbg = new Debugger;
  g.eval("function debuggee_f() { return 1; }");
  g.eval("function debuggee_g() { return 1; }");
  dbg.addDebuggee(g);
  var [ s ] = dbg.findScripts({global: g}).filter(s => s.displayName == "debuggee_f");
  var [ s2 ] = dbg.findScripts({global: g}).filter(s => s.displayName == "debuggee_g");
  g.eval("debuggee_f = null");
  gc();
  dbg.removeAllDebuggees();
  gc();
  assertEq(s.displayName, "debuggee_f");

  var wm = new WeakMap;
  var obj = Object.create(null);
  var obj2 = Object.create(null);
  wm.set(obj, s);
  wm.set(obj2, obj);
  wm.set(s2, obj2);
  s = s2 = obj = obj2 = null;

  gc();
}

enbugger();
