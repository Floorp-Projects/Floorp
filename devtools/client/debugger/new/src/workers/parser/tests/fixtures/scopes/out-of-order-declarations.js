var val;

export default function root() {
  var val;

  var fn;

  this;

  function callback() {
    console.log(val, fn, callback, root, this);

    var val;

    function fn(){};
  }

  callback();
}

import aDefault from "./src/mod";
