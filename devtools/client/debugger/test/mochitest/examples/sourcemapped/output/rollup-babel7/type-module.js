var rollupBabel7TypeModule = (function () {
  'use strict';

  var moduleScoped = 1;
  var alsoModuleScoped = 2;

  function thirdModuleScoped() {}

  function test () {
    console.log("pause here", moduleScoped, alsoModuleScoped, thirdModuleScoped);
  }

  return test;

}());
//# sourceMappingURL=type-module.js.map
