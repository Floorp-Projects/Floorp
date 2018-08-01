var rollupBabel7BabelBindingsWithFlow = (function () {
  'use strict';

  var aNamed = "a-named";

  // Webpack doesn't map import declarations well, so doing this forces binding
  function root() {
    var value = aNamed;
    console.log("pause here", root, value);
  }

  return root;

}());
//# sourceMappingURL=babel-bindings-with-flow.js.map
