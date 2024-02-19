/* eslint-disable mozilla/no-comparison-or-assignment-inside-ok */

importScripts("common_blob_reading.js");

function info(message) {
  postMessage({ type: "info", message });
}

function ok(a, message) {
  postMessage({ type: "test", test: !!a, message });
}

function is(a, b, message) {
  ok(a === b, message);
}

onmessage = function (e) {
  self[e.data.func](e.data.blob, e.data.content).then(
    () => {
      postMessage({ type: "done" });
    },
    exc => {
      dump(exc);
      dump(exc.stack);
      postMessage({ type: "error", message: exc.toString() });
    }
  );
};
