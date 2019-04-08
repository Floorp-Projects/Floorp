var rollupThisArgumentsBindings = (function () {
  'use strict';

  function root() {
    function fn(arg) {
      console.log(this, arguments);
      console.log("pause here", fn, root);

      const arrow = argArrow => {
        console.log(this, arguments);
        console.log("pause here", fn, root);
      };
      arrow("arrow-arg");
    }

    fn.call("this-value", "arg-value");
  }

  return root;

}());
//# sourceMappingURL=this-arguments-bindings.js.map
