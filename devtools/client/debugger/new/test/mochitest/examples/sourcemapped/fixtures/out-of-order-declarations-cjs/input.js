var val = "outer-value";

export default function root() {
  var val = "middle-value";
  var fn = function outerFn(outer){};

  function callback() {
    console.log("pause here", val, aDefault, fn);

    var val = "inner-value";
    function fn(inner){};
  }

  callback();
}

import aDefault from "./src/mod";
