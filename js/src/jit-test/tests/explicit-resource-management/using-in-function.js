// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management")

load(libdir + "asserts.js");

const disposed = [];
function testDisposalsInFunction() {
  using a = { [Symbol.dispose]: () => disposed.push("a") };
  using b = { [Symbol.dispose]: () => disposed.push("b") };
  using c = { [Symbol.dispose]: () => disposed.push("c") };
  disposed.push("d");
}
testDisposalsInFunction();
assertArrayEq(disposed, ["d", "c", "b", "a"]);

const disposedInFunctionAndBlock = [];
function testDisposalsInFunctionAndBlock() {
  using a = {
    [Symbol.dispose]: () => disposedInFunctionAndBlock.push("a")
  };

  using b = {
    [Symbol.dispose]: () => disposedInFunctionAndBlock.push("b")
  };

  {
    using c = {
      [Symbol.dispose]: () => disposedInFunctionAndBlock.push("c")
    };

    {
      using d = {
        [Symbol.dispose]: () => disposedInFunctionAndBlock.push("d")
      };
    }

    using e = {
      [Symbol.dispose]: () => disposedInFunctionAndBlock.push("e")
    };
  }

  using f = {
    [Symbol.dispose]: () => disposedInFunctionAndBlock.push("f")
  };

  disposedInFunctionAndBlock.push("g");
}
testDisposalsInFunctionAndBlock();
assertArrayEq(disposedInFunctionAndBlock, ["d", "e", "c", "g", "f", "b", "a"]);
