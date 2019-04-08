var rollupWebpackLineMappings = (function () {
  'use strict';

  var aDefault = "a-default";

  function root() {
    let one = 1;

    {
      const two = 2;

      var fn = function someName(arg) {
        console.log(this, arguments);
        console.log("pause here", aDefault, one, two, fn, arg);

        var inner = (arg) => { var body = "42"; console.log("pause here", body, root); };
        inner();
      };
      fn.call("this-value", "arg-value");
    }
  }

  return root;

}());
//# sourceMappingURL=webpack-line-mappings.js.map
