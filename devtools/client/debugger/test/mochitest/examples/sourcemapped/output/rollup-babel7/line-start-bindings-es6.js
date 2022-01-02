var rollupBabel7LineStartBindingsEs6 = (function () {
  'use strict';

  function root() {
    function aFunc() {
      // applies here too.

      this.thing = 4;
      console.log("pause here", root);
    }

    aFunc.call({});
  }

  return root;

}());
//# sourceMappingURL=line-start-bindings-es6.js.map
