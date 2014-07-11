// |jit-test| error: 503

if (getBuildConfiguration().parallelJS) {
  Array.buildPar(16427, function(x) {
      if (x % 633 == 503) {
          throw x;
      }
  });
} else {
  throw 503;
}
