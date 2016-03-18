oomTest(
  function x() {
    try {
      eval('let ')
    } catch (ex) {
      (function() {})()
    }
  }
);
