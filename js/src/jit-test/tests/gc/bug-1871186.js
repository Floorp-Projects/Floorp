// |jit-test| --blinterp-eager; skip-if: !('oomTest' in this)

gc();
function f(x) {
  new Uint8Array(x);
}
f(0);
oomTest(function () {
  f(2048);
});
