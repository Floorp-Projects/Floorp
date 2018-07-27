var rollupEvalMaps = (function () {
  'use strict';

  function root() {
    var one = 1;

    {
      let two = 4;
      const three = 5;

      console.log("pause here", one, two, three);
    }
  }

  return root;

}());
//# sourceMappingURL=eval-maps.js.map
