/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that we can debug workers using ES Modules.

"use strict";

const httpServer = createTestHTTPServer();
httpServer.registerContentType("html", "text/html");
httpServer.registerContentType("js", "application/javascript");

httpServer.registerPathHandler(`/`, function (request, response) {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write(`
      <html>
          <script>window.worker = new Worker("worker.js", { type: "module" })</script>
      </html>`);
});

httpServer.registerPathHandler("/worker.js", function (request, response) {
  response.setHeader("Content-Type", "application/javascript");
  response.write(`
    onmessage = () => {
      console.log("breakpoint line");
      postMessage("resume");
    }
  `);
});
const port = httpServer.identity.primaryPort;
const TEST_URL = `http://localhost:${port}/`;
const WORKER_URL = TEST_URL + "worker.js";

add_task(async function () {
  const dbg = await initDebuggerWithAbsoluteURL(TEST_URL);

  // Note that the following APIs only returns the additional threads
  await waitForThreadCount(dbg, 1);
  const threads = dbg.selectors.getThreads();
  is(threads.length, 1, "Got the page and the worker threads");
  is(threads[0].name, WORKER_URL, "Thread name is correct");

  const source = findSource(dbg, "worker.js");
  await selectSource(dbg, source);
  await addBreakpoint(dbg, source, 3);

  info("Call worker code to trigger the breakpoint");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.worker.postMessage("foo");
  });

  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 3);
  assertTextContentOnLine(dbg, 3, `console.log("breakpoint line");`);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    // Create a Promise which will resolve once the worker resumes and send a message
    // Hold the promise on window, so that a following task can use it.
    content.onWorkerResumed = new Promise(
      resolve => (content.wrappedJSObject.worker.onmessage = resolve)
    );
  });

  await resume(dbg);

  info("Wait for the worker to resume its execution and send a message");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
    ok(
      content.onWorkerResumed,
      "The promise created by the previous task exists"
    );
    await content.onWorkerResumed;
    ok(true, "The worker resumed its execution");
  });
});
