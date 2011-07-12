/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
const importURL = "relativeLoad_import.js";

onmessage = function(event) {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "testXHR.txt", false);
  xhr.send(null);
  if (xhr.status != 200 ||
      xhr.responseText != "A noisy noise annoys an oyster.") {
    throw "Couldn't get xhr text from where we wanted it!";
  }

  importScripts(importURL);
  var worker = new Worker("relativeLoad_worker2.js");
  worker.onerror = function(event) {
    throw event.message;
  };
  worker.onmessage = function(event) {
    if (event.data != workerURL) {
      throw "Bad data!";
    }
    postMessage(workerURL);
  }
};
