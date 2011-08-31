/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
onclose = function() {
  postMessage("closed");
};

setTimeout(function () {
  setTimeout(function () {
    throw new Error("I should never run!");
  }, 1000);
  close();
}, 1000);
