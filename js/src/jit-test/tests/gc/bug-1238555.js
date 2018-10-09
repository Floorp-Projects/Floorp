// |jit-test| skip-if: !('oomTest' in this)

oomTest(
  function x() {
    try {
      eval('let ')
    } catch (ex) {
      (function() {})()
    }
  }
);
