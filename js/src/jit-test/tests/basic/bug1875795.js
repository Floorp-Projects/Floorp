// |jit-test| --fast-warmup; --no-threads; skip-if: !('oomTest' in this)
oomTest(function() {
  var o = {};
  for (var p in this) {
    o[p] = 1;
  }
});
