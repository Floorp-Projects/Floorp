var rollupBabel6Esmodules = (function () {
  'use strict';

  var aDefault = "a-default";

  var aNamed = "a-named";

  var original = "an-original";

  var mod4 = "a-default";
  var aNamed$1 = "a-named";

  var aNamespace = /*#__PURE__*/Object.freeze({
    default: mod4,
    aNamed: aNamed$1
  });

  var aDefault2 = "a-default2";

  var aNamed2 = "a-named2";

  var original$1 = "an-original2";

  var aDefault3 = "a-default3";

  var aNamed3 = "a-named3";

  var original$2 = "an-original3";

  function root() {
    console.log("pause here", root);

    console.log(aDefault);
    console.log(original);
    console.log(aNamed);
    console.log(aNamed);
    console.log(aNamespace);

    try {
      // None of these are callable in this code, but we still want to make sure
      // they map properly even if the only reference is in a call expressions.
      console.log(aDefault2());
      console.log(original$1());
      console.log(aNamed2());
      console.log(aNamed2());

      console.log(new aDefault3());
      console.log(new original$2());
      console.log(new aNamed3());
      console.log(new aNamed3());
    } catch (e) {}
  }

  return root;

}());
//# sourceMappingURL=esmodules.js.map
