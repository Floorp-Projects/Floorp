// |jit-test| skip-if: !('oomTest' in this)
var x = [];
function f() {
  Object.entries(x);
  Object.defineProperty(x, "", { enumerable: true, get: f });
}
oomTest(f);
