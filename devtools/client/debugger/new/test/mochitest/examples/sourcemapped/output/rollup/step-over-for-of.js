var rollupStepOverForOf = (function () {
  'use strict';

  const vals = [1, 2];

  function root() {
    console.log("pause here");

    for (const val of vals) {
      console.log("pause again", val);
    }

    console.log("done");
  }

  return root;

}());
//# sourceMappingURL=step-over-for-of.js.map
