// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management")

load(libdir + "asserts.js");

const disposedInBlock = [];
{
  using a = { [Symbol.dispose]: () => disposedInBlock.push("a") };
  using b = { [Symbol.dispose]: () => disposedInBlock.push("b") };
}
assertArrayEq(disposedInBlock, ["b", "a"]);
