var rollupBabel6ThisArgumentsBindings = (function () {
  'use strict';

  function root() {
    function fn(arg) {
      var _this = this,
          _arguments = arguments;

      console.log(this, arguments);
      console.log("pause here", fn, root);

      var arrow = function arrow(argArrow) {
        console.log(_this, _arguments);
        console.log("pause here", fn, root);
      };
      arrow("arrow-arg");
    }

    fn.call("this-value", "arg-value");
  }

  return root;

}());
//# sourceMappingURL=this-arguments-bindings.js.map
