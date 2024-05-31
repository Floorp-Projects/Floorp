// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management")

load(libdir + "asserts.js");

let disposedInIf = [];
function testDisposalsInIf(ifTrue) {
  using a = {
    [Symbol.dispose]: () => disposedInIf.push("a")
  };

  if (ifTrue) {
    using b = {
      [Symbol.dispose]: () => disposedInIf.push("b")
    };
  } else {
    using c = {
      [Symbol.dispose]: () => disposedInIf.push("c")
    };
  }

  using d = {
    [Symbol.dispose]: () => disposedInIf.push("d")
  };

  disposedInIf.push("e");
}
testDisposalsInIf(true);
assertArrayEq(disposedInIf, ["b", "e", "d", "a"]);
disposedInIf = [];
testDisposalsInIf(false);
assertArrayEq(disposedInIf, ["c", "e", "d", "a"]);
