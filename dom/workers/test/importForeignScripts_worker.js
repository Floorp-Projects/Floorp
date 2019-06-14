/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var target = self;
var response;

function runTests() {
  response = "good";
  try {
    importScripts("http://example.org/tests/dom/workers/test/foreign.js");
  } catch(e) {
    dump("Got error " + e + " when calling importScripts");
  }
  if (response === "good") {
    try {
      importScripts("redirect_to_foreign.sjs");
    } catch(e) {
      dump("Got error " + e + " when calling importScripts");
    }
  }
  target.postMessage(response);

  // Now, test a nested worker.
  if (location.search !== "?nested") {
    var worker = new Worker("importForeignScripts_worker.js?nested");

    worker.onmessage = function(e) {
      target.postMessage(e.data);
      target.postMessage("finish");
    }

    worker.onerror = function() {
      target.postMessage("nested worker error");
    }

    worker.postMessage("start");
  }
}

onmessage = function(e) {
  if (e.data === "start") {
    runTests();
  }
};

onconnect = function(e) {
  target = e.ports[0];
  e.ports[0].onmessage = function(msg) {
    if (msg.data === "start") {
      runTests();
    }
  };
};
