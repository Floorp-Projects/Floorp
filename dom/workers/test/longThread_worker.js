/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
onmessage = function(event) {
  switch (event.data) {
    case "start":
      for (var i = 0; i < 10000000; i++) { };
      postMessage("done");
      break;
    default:
      throw "Bad message: " + event.data;
  }
};
