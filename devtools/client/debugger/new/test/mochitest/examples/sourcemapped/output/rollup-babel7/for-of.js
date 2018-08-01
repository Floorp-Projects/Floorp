var rollupBabel7ForOf = (function () {
  'use strict';

  function forOf() {
    var _arr = [1];

    for (var _i = 0; _i < _arr.length; _i++) {
      var x = _arr[_i];
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
