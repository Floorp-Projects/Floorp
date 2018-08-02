var rollupBabel7StepOverForOfClosure = (function () {
  'use strict';

  // Babel will convert the loop body to a function to handle the 'val' lexical
  // enclosure behavior.
  var vals = [1, 2];
  function root() {
    console.log("pause here");

    var _loop = function _loop() {
      var val = vals[_i];
      console.log("pause again", function () {
        return val;
      }());
    };

    for (var _i = 0; _i < vals.length; _i++) {
      _loop();
    }

    console.log("done");
  }

  return root;

}());
//# sourceMappingURL=step-over-for-of-closure.js.map
