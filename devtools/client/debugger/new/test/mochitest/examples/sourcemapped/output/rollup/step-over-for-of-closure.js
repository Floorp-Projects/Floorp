var rollupStepOverForOfClosure = (function () {
  'use strict';

  // Babel will convert the loop body to a function to handle the 'val' lexical
  // enclosure behavior.
  const vals = [1, 2];

  function root() {
    console.log("pause here");

    for (const val of vals) {
      console.log("pause again", (() => val)());
    }

    console.log("done");
  }

  return root;

}());
//# sourceMappingURL=step-over-for-of-closure.js.map
