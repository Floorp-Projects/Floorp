// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management")

load(libdir + "asserts.js");

const disposedInForOf = [];
async function testDisposalsInForOfWithAsyncIter() {
  async function* asyncGenerator() {
    yield {
      value: "a",
      [Symbol.dispose]: () => disposedInForOf.push("disposed a")
    };
    yield {
      value: "b",
      [Symbol.dispose]: () => disposedInForOf.push("disposed b")
    };
    yield {
      value: "c",
      [Symbol.dispose]: () => disposedInForOf.push("disposed c")
    };
  }

  for await (using disposable of asyncGenerator()) {
    disposedInForOf.push(disposable.value);
  }
}

testDisposalsInForOfWithAsyncIter();
drainJobQueue();
assertArrayEq(disposedInForOf, ["a", "disposed a", "b", "disposed b", "c", "disposed c"]);
