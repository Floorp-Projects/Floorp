/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
onclose = function() {
  postMessage("closed");
};

setTimeout(function() { close(); }, 1000);
