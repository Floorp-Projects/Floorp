// |jit-test| --fast-warmup
function f(s) {
  // Trial-inline self-hosted |replace| and relazify.
  for (var i = 0; i < 50; i++) {
    s = s.replace("a", "b");
  }
  trialInline();
  relazifyFunctions();

  // Warp-compile.
  for (var j = 0; j < 50; j++) {}

  return s;
}
assertEq(f("a"), "b");
