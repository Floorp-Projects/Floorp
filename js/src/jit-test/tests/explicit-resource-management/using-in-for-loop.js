// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management")

load(libdir + "asserts.js");

const disposedInLoop = [];
function testDisposalsInForLoop() {
  using a = {
    [Symbol.dispose]: () => disposedInLoop.push("a")
  };

  for (let i = 0; i < 3; i++) {
    using b = {
      [Symbol.dispose]: () => disposedInLoop.push(i)
    };
  }

  using c = {
    [Symbol.dispose]: () => disposedInLoop.push("c")
  };
}
testDisposalsInForLoop();
assertArrayEq(disposedInLoop, [0, 1, 2, "c", "a"]);

const disposedInForInLoop = [];
function testDisposalsInForInLoop() {
  using a = {
    [Symbol.dispose]: () => disposedInForInLoop.push("a")
  };

  for (let i of [0, 1]) {
    using x = {
      [Symbol.dispose]: () => {
        disposedInForInLoop.push(i);
      }
    };
  }
}
testDisposalsInForInLoop();
assertArrayEq(disposedInForInLoop, [0, 1, "a"]);
