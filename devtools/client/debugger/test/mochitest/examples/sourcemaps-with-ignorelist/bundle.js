(function () {
  'use strict';

  function original1() {
    debugger;
    console.log("The first original source");
  }

  function original2() {
    debugger;
    console.log("The second original source");
  }

  function original3() {
    debugger;
    console.log("The third original source");
  }

  function original4() {
    debugger;
    console.log("The fourth original source");
  }

  function original5() {
    console.log("The fifth original source");
  }

  original1();
  original2();
  original3();
  original4();
  original5();

})();
//# sourceMappingURL=bundle.js.map
