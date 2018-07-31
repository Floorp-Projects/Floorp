var rollupForLoops = (function () {
  'use strict';

  function root() {

    for (let i = 1; i < 2; i++) {
      console.log("pause here", root);
    }

    for (let i in { 2: "" }) {
      console.log("pause here", root);
    }

    for (let i of [3]) {
      console.log("pause here", root);
    }
  }

  return root;

}());
//# sourceMappingURL=for-loops.js.map
