/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

export async function runTestInWorker(script, base, listener) {
  return new Promise(function (resolve) {
    const globalHeadUrl = new URL(
      "/tests/dom/quota/test/modules/worker/head.js",
      base
    );

    let modules = {};

    const worker = new Worker(globalHeadUrl.href);

    worker.onmessage = async function (event) {
      const data = event.data;
      const moduleName = data.moduleName;
      const objectName = data.objectName;

      if (moduleName && objectName) {
        if (!modules[moduleName]) {
          // eslint-disable-next-line no-unsanitized/method
          modules[moduleName] = await import(
            "/tests/dom/quota/test/modules/" + moduleName + ".mjs"
          );
        }
        await modules[moduleName][objectName].OnMessageReceived(worker, data);
        return;
      }

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

    worker.onerror = function (event) {
      listener.onOk(false, "Worker had an error: " + event.data);
      resolve();
    };

    const scriptUrl = new URL(script, base);

    const localHeadUrl = new URL("head.js", scriptUrl);

    worker.postMessage([localHeadUrl.href, scriptUrl.href]);
  });
}
