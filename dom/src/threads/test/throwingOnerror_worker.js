onerror = function(event) {
  throw "bar";
};

var count = 0;
onmessage = function(event) {
  if (!count++) {
    throw "foo";
  }
  postMessage("");
};
