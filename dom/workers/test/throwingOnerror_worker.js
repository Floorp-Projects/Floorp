/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
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
