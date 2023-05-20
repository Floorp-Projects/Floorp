/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-env mozilla/chrome-worker */
/* global OS */

function log(text) {
  dump("WORKER " + text + "\n");
}

function send(message) {
  self.postMessage(message);
}

self.onmessage = function (msg) {
  self.onmessage = function (msgInner) {
    log("ignored message " + JSON.stringify(msgInner.data));
  };
  let { umask } = msg.data;
  try {
    test_umaskWorkerThread(umask);
  } catch (x) {
    log("Catching error: " + x);
    log("Stack: " + x.stack);
    log("Source: " + x.toSource());
    ok(false, x.toString() + "\n" + x.stack);
  }
  finish();
};

function finish() {
  send({ kind: "finish" });
}

function ok(condition, description) {
  send({ kind: "ok", condition, description });
}
function is(a, b, description) {
  send({ kind: "is", a, b, description });
}
function isnot(a, b, description) {
  send({ kind: "isnot", a, b, description });
}

// Test that OS.Constants.Sys.umask is set properly in ChromeWorker thread
function test_umaskWorkerThread(umask) {
  is(
    umask,
    OS.Constants.Sys.umask,
    "OS.Constants.Sys.umask is set properly on worker thread: " +
      ("0000" + umask.toString(8)).slice(-4)
  );
}
