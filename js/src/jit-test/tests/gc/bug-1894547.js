// |jit-test| --enable-symbols-as-weakmap-keys; skip-if: getBuildConfiguration("release_or_beta")

gczeal(0);
let wm = new WeakMap();
let s = {};
wm.set(s, new WeakMap());
let ss = {x: Symbol()};
wm.get(s).set(this, ss);
let wm2 = new WeakMap();
wm2.set(ss, "test");
ss = null;

// Collect only this zone and not the zone containing the symbol.
gc({});
