function f() {
    Array.buildPar(6, function() {});
    f();
}

if (getBuildConfiguration().parallelJS)
  f();
