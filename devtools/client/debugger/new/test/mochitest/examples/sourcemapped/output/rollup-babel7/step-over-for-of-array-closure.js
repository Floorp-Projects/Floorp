var rollupBabel7StepOverForOfArrayClosure = (function () {
  'use strict';

  // Babel will optimize this for..of because it call tell the value is an array.
  function root() {
    console.log("pause here");
    var _arr = [1, 2];

    var _loop = function _loop() {
      var val = _arr[_i];
      console.log("pause again", function () {
        return val;
      }());
    };

    for (var _i = 0; _i < _arr.length; _i++) {
      _loop();
    }

    console.log("done");
  }

  return root;

}());
//# sourceMappingURL=step-over-for-of-array-closure.js.map
