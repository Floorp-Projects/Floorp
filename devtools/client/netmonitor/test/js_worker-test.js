/* eslint-disable no-unused-vars, no-undef */
"use strict";
startWorkerFromWorker();

var w;
function startWorkerFromWorker() {
  w = new Worker("js_worker-test2.js");
}

importScriptsFromWorker();

function importScriptsFromWorker() {
  try {
    importScripts("missing1.js", "missing2.js");
  } catch (e) {}
}

createJSONRequest();

function createJSONRequest() {
  const request = new XMLHttpRequest();
  request.open("GET", "missing.json", true);
  request.send(null);
}
