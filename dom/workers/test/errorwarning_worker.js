/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function errorHandler() {
  postMessage({ type: "error" });
}

onmessage = function (event) {
  if (event.data.errors) {
    try {
      // This is an error:
      postMessage({ type: "ignore", value: b.aaa });
    } catch (e) {
      errorHandler();
    }
  } else {
    var a = {};
    // This is a warning:
    postMessage({ type: "ignore", value: a.foo });
  }

  if (event.data.loop != 0) {
    var worker = new Worker("errorwarning_worker.js");
    worker.onerror = errorHandler;
    worker.postMessage({
      loop: event.data.loop - 1,
      errors: event.data.errors,
    });

    worker.onmessage = function (e) {
      postMessage(e.data);
    };
  } else {
    postMessage({ type: "finish" });
  }
};

onerror = errorHandler;
// eslint-disable-next-line no-self-assign
onerror = onerror;
// eslint-disable-next-line no-self-compare
if (!onerror || onerror != onerror) {
  throw "onerror wasn't set properly";
}
