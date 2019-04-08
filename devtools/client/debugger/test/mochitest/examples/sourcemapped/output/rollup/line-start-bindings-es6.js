var rollupLineStartBindingsEs6 = (function () {
  'use strict';

  function root() {
    function aFunc(){

      // The 'this' binding here is also its own line, so the comment above
      // applies here too.
      this.thing = 4;

      console.log("pause here", root);
    }

    aFunc.call({ });
  }

  return root;

}());
//# sourceMappingURL=line-start-bindings-es6.js.map
