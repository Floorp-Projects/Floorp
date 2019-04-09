var rollupBabel6StepOverForOfClosure = (function () {
  'use strict';

  // Babel will convert the loop body to a function to handle the 'val' lexical
  // enclosure behavior.
  var vals = [1, 2];

  function root() {
    console.log("pause here");

    var _loop = function _loop(val) {
      console.log("pause again", function () {
        return val;
      }());
    };

    var _iteratorNormalCompletion = true;
    var _didIteratorError = false;
    var _iteratorError = undefined;

    try {
      for (var _iterator = vals[Symbol.iterator](), _step; !(_iteratorNormalCompletion = (_step = _iterator.next()).done); _iteratorNormalCompletion = true) {
        var val = _step.value;

        _loop(val);
      }
    } catch (err) {
      _didIteratorError = true;
      _iteratorError = err;
    } finally {
      try {
        if (!_iteratorNormalCompletion && _iterator.return) {
          _iterator.return();
        }
      } finally {
        if (_didIteratorError) {
          throw _iteratorError;
        }
      }
    }

    console.log("done");
  }

  return root;

}());
//# sourceMappingURL=step-over-for-of-closure.js.map
