var rollupForOf = (function () {
  'use strict';

  function forOf() {
    for (const x of [1]) {
      doThing(x);
    }

    function doThing(arg) {
      // Avoid optimize out
      window.console;
    }
  }

  return forOf;

}());
//# sourceMappingURL=for-of.js.map
