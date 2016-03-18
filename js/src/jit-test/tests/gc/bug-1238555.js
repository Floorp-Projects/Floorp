if (!('oomTest' in this))
  quit();

oomTest(
  function x() {
    try {
      eval('let ')
    } catch (ex) {
      (function() {})()
    }
  }
);
