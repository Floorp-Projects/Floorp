var rollupBabel7StepOverFunctionParams = (function () {
  'use strict';

  function test(a1) {

    var _ref = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : {},
        a3 = _ref.a3,
        a4 = _ref.a4,
        _ref$a = _ref.a5;

    _ref$a = _ref$a === void 0 ? {} : _ref$a;
    var a7 = _ref$a.a6;
    console.log("pause next here");
  }

  function fn() {
    console.log("pause here");
    test("1", undefined, {
      a3: "3",
      a4: "4",
      a5: {
        a6: "7"
      }
    });
  }

  return fn;

}());
//# sourceMappingURL=step-over-function-params.js.map
