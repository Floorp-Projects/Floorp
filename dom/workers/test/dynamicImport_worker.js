/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

onmessage = function (event) {
  switch (event.data) {
    case "start":
      import("./dynamicImport_nested.mjs").then(m => postMessage(m.message));
      break;
    default:
      throw new Error("Bad message: " + event.data);
  }
};
