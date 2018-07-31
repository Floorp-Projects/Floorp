var rollupWebpackFunctions = (function () {
  'use strict';

  var module$1 = {};

  module$1.exports = function(x) {
    return x * 2;
  };


  function root() {
    // This example is structures to look like CommonJS in order to replicate
    // a previously-encountered bug.
    module$1.exports(4);
  }

  return root;

}());
//# sourceMappingURL=webpack-functions.js.map
