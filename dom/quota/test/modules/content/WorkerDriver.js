/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

export async function runTestInWorker(script) {
  return new Promise(function(resolve) {
    const globalHeadUrl = new URL(
      "/tests/dom/quota/test/modules/worker/head.js",
      window.location.href
    );

    const worker = new Worker(globalHeadUrl.href);

    worker.onmessage = function(event) {
      const data = event.data;

      switch (data.op) {
        case "ok":
          ok(data.value, data.message);
          break;

        case "is":
          is(data.a, data.b, data.message);
          break;

        case "info":
          info(data.message);
          break;

        case "finish":
          resolve();
          break;

        case "failure":
          ok(false, "Worker had a failure: " + data.message);
          resolve();
          break;
      }
    };

    worker.onerror = function(event) {
      ok(false, "Worker had an error: " + event.data);
      resolve();
    };

    const scriptUrl = new URL(script, window.location.href);

    const localHeadUrl = new URL("head.js", scriptUrl);

    worker.postMessage([localHeadUrl.href, scriptUrl.href]);
  });
}
