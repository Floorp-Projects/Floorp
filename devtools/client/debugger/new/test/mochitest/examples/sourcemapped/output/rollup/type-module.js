var rollupTypeModule = (function () {
  'use strict';

  var moduleScoped = 1;
  let alsoModuleScoped = 2;

  function thirdModuleScoped() {}

  function test() {
    console.log("pause here", moduleScoped, alsoModuleScoped, thirdModuleScoped);
  }

  return test;

}());
//# sourceMappingURL=type-module.js.map
