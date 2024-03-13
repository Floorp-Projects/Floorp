var x = newGlobal().Int8Array;
for (let i = 0; i < 2; i++) {
  function f() {}
  oomTest(function() {
    new x().__proto__ = f;
  });
}
