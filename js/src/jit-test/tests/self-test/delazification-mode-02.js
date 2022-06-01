//|jit-test| skip-if: isLcovEnabled() || helperThreadCount() === 0

// GCs might trash the stencil cache. Prevent us from scheduling too many GCs.
if ('gczeal' in this) {
    gczeal(0);
}

let source = `
  function foo() {
    return "foo";
  }

  waitForStencilCache(foo);
  // false would be expected if threads are disabled.
  assertEq(isInStencilCache(foo), true);
`;

const options = {
    fileName: "inner-02.js",
    lineNumber: 1,
    eagerDelazificationStrategy: "ConcurrentDepthFirst",
    newContext: true,
};
evaluate(source, options);
