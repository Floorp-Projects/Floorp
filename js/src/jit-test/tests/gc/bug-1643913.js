gczeal(0);

for (let p of [false, true]) {
  f(p);

  // Run an incremental GC to completion.
  startgc(1);
  while (gcstate() !== 'NotActive') {
    gcslice(10000, { dontStart: true });
  }
}

function ccwToObject() {
  return evalcx('({})', newGlobal({newCompartment: true}));
}

function ccwToRegistry() {
  return evalcx('new FinalizationRegistry(value => {})',
                newGlobal({newCompartment: true}));
}

function f(p) {
  let registry = ccwToRegistry();
  let target = ccwToObject();
  registry.register(target, undefined);

  // Add a CCW from registry to target zone or vice versa to control
  // the order the zones are swept in.
  if (p) {
    registry.ptr = target;
  } else {
    target.ptr = registry;
  }

  gc();
}
