var rollupBabel7Functions = (function () {
  'use strict';

  function root() {
    function decl(p1) {
      var inner = function inner(p2) {
        var arrow = function arrow(p3) {
          console.log("pause here", p3, arrow, p2, inner, p1, decl, root);
        };

        arrow();
      };

      inner();
    }

    decl();
  }

  return root;

}());
//# sourceMappingURL=functions.js.map
