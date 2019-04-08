var rollupStepOverFunctionParams = (function () {
  'use strict';

  function test(a1, a2 = 45, { a3, a4, a5: { a6: a7 } = {} } = {}) {
    console.log("pause next here");
  }

  function fn() {
    console.log("pause here");
    test("1", undefined, { a3: "3", a4: "4", a5: { a6: "7" } });
  }

  return fn;

}());
//# sourceMappingURL=step-over-function-params.js.map
