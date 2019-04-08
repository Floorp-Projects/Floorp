var rollupBabel7TryCatches = (function () {
  'use strict';

  function root() {

    try {
      throw "AnError";
    } catch (err) {
      console.log("pause here", root);
    }
  }

  return root;

}());
//# sourceMappingURL=try-catches.js.map
