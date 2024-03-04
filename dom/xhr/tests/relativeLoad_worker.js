/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
/* eslint-env worker */
/* global workerURL */
const importURL = "relativeLoad_import.js";

onmessage = function () {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "worker_testXHR.txt", false);
  xhr.send(null);
  if (
    xhr.status != 200 ||
    xhr.responseText != "A noisy noise annoys an oyster."
  ) {
    throw new Error("Couldn't get xhr text from where we wanted it!");
  }

  importScripts(importURL);
  var worker = new Worker("relativeLoad_worker2.js");
  worker.onerror = function (e) {
    throw e.message;
  };
  worker.onmessage = function (e) {
    if (e.data != workerURL) {
      throw new Error("Bad data!");
    }
    postMessage(workerURL);
  };
};
