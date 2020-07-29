var g = newGlobal({ newCompartment: true });
g.eval(`
  var obj = {};
  var ref = new WeakRef(obj);
  Promise.resolve().then(() => {
    assertEq(ref.deref(), obj);
  });
`);
nukeCCW(g.ref);
drainJobQueue();
