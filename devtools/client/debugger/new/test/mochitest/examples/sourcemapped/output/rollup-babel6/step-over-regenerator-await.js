var rollupBabel6StepOverRegeneratorAwait = (function () {
  'use strict';

  function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

  var fn = function () {
    var _ref = _asyncToGenerator( /*#__PURE__*/regeneratorRuntime.mark(function _callee() {
      return regeneratorRuntime.wrap(function _callee$(_context) {
        while (1) {
          switch (_context.prev = _context.next) {
            case 0:
              console.log("pause here");

              _context.next = 3;
              return doAsync();

            case 3:

              console.log("stopped here");

            case 4:
            case "end":
              return _context.stop();
          }
        }
      }, _callee, this);
    }));

    function fn() {
      return _ref.apply(this, arguments);
    }

    return fn;
  }();

  function doAsync() {
    return Promise.resolve();
  }

  function root() {
    fn();
  }

  return root;

}());
//# sourceMappingURL=step-over-regenerator-await.js.map
