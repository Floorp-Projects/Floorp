var rollupBabel7StepOverForOf = (function () {
  'use strict';

  var vals = [1, 2];
  function root() {
    console.log("pause here");

    for (var _i = 0; _i < vals.length; _i++) {
      var val = vals[_i];
      console.log("pause again", val);
    }

    console.log("done");
  }

  return root;

}());
//# sourceMappingURL=step-over-for-of.js.map
