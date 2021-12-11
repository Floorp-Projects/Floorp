/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

globalThis.onmessage = function(e) {
  const { type, message } = e.data;

  if (type === "log-in-worker") {
    // Printing `e` so we can check that we have an object and not a stringified version
    console.log("[WORKER]", message, e);
  }
};
