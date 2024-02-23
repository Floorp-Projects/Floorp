// |jit-test| --blinterp-eager

gc();
function f(x) {
  new Uint8Array(x);
}
f(0);
oomTest(function () {
  f(2048);
});
