// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management")

globalThis.called = false;

const m = parseModule(`
using x = {
  [Symbol.dispose]() {
    globalThis.called = true;
  }
}
`);

moduleLink(m);
moduleEvaluate(m);

assertEq(globalThis.called, true);
