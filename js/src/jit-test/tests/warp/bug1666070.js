// |jit-test| --fast-warmup
function f() {}
for (var i = 0; i < 15; i++) {
  f();
  var g = newGlobal();
  g.trialInline();
}
trialInline();
