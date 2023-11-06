/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

onconnect = function (evt) {
  console.profile("Hello profiling from a SharedWorker!");
  console.log("Hello world from a SharedWorker!");
  console.log("Here is a SAB", new SharedArrayBuffer(1024));
  evt.ports[0].postMessage("ok!");
};
