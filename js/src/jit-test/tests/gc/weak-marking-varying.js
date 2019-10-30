// |jit-test| allow-unhandlable-oom

// Crash test. Try lots of different combinations of mark colors and
// sequencing, and rely on the in-code asserts to detect problems.

// This test requires precise control over GC timing. The SM(cgc) job will fail
// without this, because startgc() asserts if an incremental GC is already in
// progress.
gczeal(0);

function global(where) {
  if (where == 'same-realm')
    return globalThis;
  if (where == 'same-compartment')
    return newGlobal();
  if (where == 'same-zone')
    newGlobal({sameZoneAs: {}});
  return newGlobal({newCompartment: true});
}

function varying(mapColor, keyColor, delegateColor, order, where) {
  const vals = {};
  const g = global(where);

  vals.m = new WeakMap();
  vals.key = g.eval("Object.create(null)");
  vals.val = Object.create(null);
  vals.m.set(vals.key, vals.val);

  addMarkObservers([vals.m, vals.key]);
  g.delegate = vals.key;
  g.eval('addMarkObservers([delegate])');
  g.delegate = null;
  addMarkObservers([vals.val]);

  for (const action of order) {
    if (action == 'key' && ['black', 'gray'].includes(keyColor)) {
      enqueueMark(`set-color-${keyColor}`);
      enqueueMark(vals.key);
      enqueueMark("unset-color");
      enqueueMark("yield");
    } else if (action == 'map' && ['black', 'gray'].includes(mapColor)) {
      enqueueMark(`set-color-${mapColor}`);
      enqueueMark(vals.m);
      enqueueMark("drain");
      enqueueMark("unset-color");
      enqueueMark("yield");
    } else if (action == 'delegate' && ['black', 'gray'].includes(delegateColor)) {
      g.delegate = vals.key;
      g.eval(`enqueueMark("set-color-${delegateColor}")`);
      g.eval('enqueueMark(delegate)');
      g.eval('enqueueMark("unset-color")');
      g.eval('enqueueMark("yield")');
      g.delegate = null;
    }
  }

  vals.m = vals.key = vals.val = null;

  if (delegateColor == 'uncollected')
    schedulezone({});
  startgc(100000);
  print('  ' + getMarks().join("/"));
  gcslice(100000);
  print('  ' + getMarks().join("/"));
  finishgc();
  print('  ' + getMarks().join("/"));

  clearMarkQueue();
  clearMarkObservers();
}

if (this.enqueueMark) {
  for (const mapColor of ['gray', 'black']) {
    for (const keyColor of ['gray', 'black', 'unmarked']) {
      for (const delegateColor of ['gray', 'black', 'unmarked', 'uncollected']) {
        for (const order of [['map', 'key'],
                             ['key', 'map'],
                             ['map', 'delegate'],
                             ['delegate', 'map']])
        {
          for (const where of ['same-realm', 'same-compartment', 'same-zone', 'other-zone']) {
            print(`\nRunning variant map/key/delegate=${mapColor}/${keyColor}/${delegateColor}, key is ${where}, order: ${order.join(" ")}`);
            varying(mapColor, keyColor, delegateColor, order, where);
          }
        }
      }
    }
  }
}
