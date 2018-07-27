var rollupLexAndNonlex = (function () {
  'use strict';

  function root() {
    function someHelper(){
      console.log("pause here", root, Thing);
    }

    class Thing {}

    someHelper();
  }

  return root;

}());
//# sourceMappingURL=lex-and-nonlex.js.map
