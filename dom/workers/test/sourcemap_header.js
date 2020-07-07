/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

(async () => {
  SimpleTest.waitForExplicitFinish();

  const HTTP_BASE_URL = "http://mochi.test:8888/tests/dom/workers/test/";
  const IFRAME_URL = HTTP_BASE_URL + "sourcemap_header_iframe.html";
  const WORKER_URL = HTTP_BASE_URL + "sourcemap_header_worker.js";
  const DEBUGGER_URL = BASE_URL + "sourcemap_header_debugger.js";

  const workerFrame = document.getElementById("worker-frame");
  ok(workerFrame, "has frame");

  await new Promise(r => {
    workerFrame.onload = r;
    workerFrame.src = IFRAME_URL;
  });

  info("Start worker and watch for registration");
  const workerLoadedChannel = new MessageChannel();

  const loadDebuggerAndWorker = Promise.all([
    waitForRegister(WORKER_URL, DEBUGGER_URL),
    // We need to wait for the worker to load so a Debugger.Source will be
    // guaranteed to exist.
    new Promise(r => {
      workerLoadedChannel.port1.onmessage = r;
    }),
  ]);
  workerFrame.contentWindow.postMessage(WORKER_URL, "*", [
    workerLoadedChannel.port2,
  ]);
  const [dbg] = await loadDebuggerAndWorker;

  // Wait for the debugger server to reply with the sourceMapURL of the
  // loaded worker scripts.
  info("Querying for the sourceMapURL of the worker script");
  const urls = await new Promise(res => {
    dbg.addListener({
      onMessage(msg) {
        const data = JSON.parse(msg);
        if (data.type !== "response-sourceMapURL") {
          return;
        }
        dbg.removeListener(this);
        res(data.value);
      },
    });
    dbg.postMessage(
      JSON.stringify({
        type: "request-sourceMapURL",
        url: WORKER_URL,
      })
    );
  });

  ok(Array.isArray(urls) && urls.length === 1, "has a single source actor");
  is(urls[0], "worker-header.js.map", "has the right map URL");

  SimpleTest.finish();
})();
