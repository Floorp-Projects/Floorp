// |jit-test| skip-if: !('oomTest' in this)
enableShapeConsistencyChecks();
oomTest(() => {
  var obj = {a: 1, b: 2, c: 3};
  delete obj.a;
  for (var i = 0; i < 100; i++) {
    obj["x" + i] = i;
  }
});
