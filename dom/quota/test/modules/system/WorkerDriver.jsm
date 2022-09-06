/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

async function runTestInWorker(script, base, listener) {
  return new Promise(function(resolve) {
    const globalHeadUrl = new URL(
      "resource://testing-common/dom/quota/test/modules/worker/head.js"
    );

    const worker = new Worker(globalHeadUrl.href);

    worker.onmessage = function(event) {
      const data = event.data;

      switch (data.op) {
        case "ok":
          listener.onOk(data.value, data.message);
          break;

        case "is":
          listener.onIs(data.a, data.b, data.message);
          break;

        case "info":
          listener.onInfo(data.message);
          break;

        case "finish":
          resolve();
          break;

        case "failure":
          listener.onOk(false, "Worker had a failure: " + data.message);
          resolve();
          break;
      }
    };

    worker.onerror = function(event) {
      listener.onOk(false, "Worker had an error: " + event.data);
      resolve();
    };

    const scriptUrl = new URL(script, base);

    const localHeadUrl = new URL("head.js", scriptUrl);

    worker.postMessage([localHeadUrl.href, scriptUrl.href]);
  });
}

const EXPORTED_SYMBOLS = ["runTestInWorker"];
