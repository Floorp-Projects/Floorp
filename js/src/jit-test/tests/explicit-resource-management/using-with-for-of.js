// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management")

load(libdir + "asserts.js");

const disposedInForOf = [];
function testDisposalsInForOf() {
  const disposables = [
    {
      value: "a",
      [Symbol.dispose]: () => disposedInForOf.push("disposed a")
    },
    {
      value: "b",
      [Symbol.dispose]: () => disposedInForOf.push("disposed b")
    },
    {
      value: "c",
      [Symbol.dispose]: () => disposedInForOf.push("disposed c")
    }
  ];

  for (using disposable of disposables) {
    disposedInForOf.push(disposable.value);
  }
}

testDisposalsInForOf();
assertArrayEq(disposedInForOf, ["a", "disposed a", "b", "disposed b", "c", "disposed c"]);
