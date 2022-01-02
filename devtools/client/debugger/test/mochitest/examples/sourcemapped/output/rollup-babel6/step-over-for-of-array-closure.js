var rollupBabel6StepOverForOfArrayClosure = (function () {
  'use strict';

  // Babel will optimize this for..of because it call tell the value is an array.
  function root() {
    console.log("pause here");

    var _loop = function _loop(val) {
      console.log("pause again", function () {
        return val;
      }());
    };

    var _arr = [1, 2];
    for (var _i = 0; _i < _arr.length; _i++) {
      var val = _arr[_i];
      _loop(val);
    }

    console.log("done");
  }

  return root;

}());
//# sourceMappingURL=step-over-for-of-array-closure.js.map
