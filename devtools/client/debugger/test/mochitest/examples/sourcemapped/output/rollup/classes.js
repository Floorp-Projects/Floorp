var rollupClasses = (function () {
  'use strict';

  function root() {
    let one = 1;

    class Thing {
      one() {
        console.log("pause here");
      }
    }

    class Another {
      method() {

        console.log("pause here", Another, one, Thing, root);
      }
    }

    new Another().method();
    new Thing().one();
  }

  return root;

}());
//# sourceMappingURL=classes.js.map
