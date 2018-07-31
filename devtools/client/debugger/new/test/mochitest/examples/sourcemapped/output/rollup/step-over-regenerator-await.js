var rollupStepOverRegeneratorAwait = (function () {
  'use strict';

  var fn = async function fn() {
    console.log("pause here");

    await doAsync();

    console.log("stopped here");
  };

  function doAsync() {
    return Promise.resolve();
  }

  function root() {
    fn();
  }

  return root;

}());
//# sourceMappingURL=step-over-regenerator-await.js.map
