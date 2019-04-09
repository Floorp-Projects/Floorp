var rollupStepOverForOfArrayClosure = (function () {
  'use strict';

  // Babel will optimize this for..of because it call tell the value is an array.
  function root() {
    console.log("pause here");

    for (const val of [1, 2]) {
      console.log("pause again", (() => val)());
    }

    console.log("done");
  }

  return root;

}());
//# sourceMappingURL=step-over-for-of-array-closure.js.map
