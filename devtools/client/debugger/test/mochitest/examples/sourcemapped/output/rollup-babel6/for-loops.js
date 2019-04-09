var rollupBabel6ForLoops = (function () {
  'use strict';

  function root() {

    for (var _i = 1; _i < 2; _i++) {
      console.log("pause here", root);
    }

    for (var _i2 in { 2: "" }) {
      console.log("pause here", root);
    }

    var _arr = [3];
    for (var _i4 = 0; _i4 < _arr.length; _i4++) {
      console.log("pause here", root);
    }
  }

  return root;

}());
//# sourceMappingURL=for-loops.js.map
