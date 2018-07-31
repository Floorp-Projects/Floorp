var rollupShadowedVars = (function () {
  'use strict';

  function test() {
    {
      {

        console.log("pause here");
      }
    }
  }

  return test;

}());
//# sourceMappingURL=shadowed-vars.js.map
