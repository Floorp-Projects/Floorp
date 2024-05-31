// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management")

load(libdir + "asserts.js");

const disposedInGenerator = [];
function* gen() {
  using x = {
    value: 1,
    [Symbol.dispose]() {
      disposedInGenerator.push(42);
    }
  };
  yield x;
}
function testDisposalsInGenerator() {
  let iter = gen();
  iter.next();
  iter.next();
  disposedInGenerator.push(43);
}
testDisposalsInGenerator();
assertArrayEq(disposedInGenerator, [42, 43]);
