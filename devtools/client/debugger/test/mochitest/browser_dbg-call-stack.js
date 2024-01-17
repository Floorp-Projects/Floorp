/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Ignore strange errors when shutting down.
PromiseTestUtils.allowMatchingRejectionsGlobally(/No such actor/);

add_task(async function () {
  const dbg = await initDebugger("doc-script-switching.html");

  const found = findElement(dbg, "callStackBody");
  is(found, null, "Call stack is hidden");

  invokeInTab("firstCall");
  await waitForPaused(dbg);
  ok(isFrameSelected(dbg, 1, "secondCall"), "the first frame is selected");

  const button = toggleButton(dbg);
  ok(!button, "toggle button shouldn't be there");
});

add_task(async function () {
  const dbg = await initDebugger("doc-frames.html");

  invokeInTab("startRecursion");
  await waitForPaused(dbg);
  ok(isFrameSelected(dbg, 1, "recurseA"), "the first frame is selected");

  // check to make sure that the toggle button isn't there
  let button = toggleButton(dbg);
  let frames = findAllElements(dbg, "frames");
  is(button.innerText, "Expand rows", "toggle button should be 'expand'");
  is(frames.length, 7, "There should be at most seven frames");

  button.click();

  button = toggleButton(dbg);
  frames = findAllElements(dbg, "frames");
  is(button.innerText, "Collapse rows", "toggle button should be collapsed");
  is(frames.length, 22, "All of the frames should be shown");
  await waitForSelectedSource(dbg, "frames.js");
});

add_task(async function () {
  const url = createMockAngularPage();
  const tab = await addTab(url);
  info("Open debugger");
  const toolbox = await openToolboxForTab(tab, "jsdebugger");
  const dbg = createDebuggerContext(toolbox);

  const found = findElement(dbg, "callStackBody");
  is(found, null, "Call stack is hidden");

  const pausedContent = SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    function () {
      content.document.querySelector("button.pause").click();
    }
  );

  await waitForPaused(dbg);
  const $group = findElementWithSelector(dbg, ".frames .frames-group");
  is(
    $group.querySelector(".badge").textContent,
    "2",
    "Group has expected badge"
  );
  is(
    $group.querySelector(".group-description-name").textContent,
    "Angular",
    "Group has expected location"
  );

  await resume(dbg);

  info("Wait for content to be resumed");
  await pausedContent;
});

function toggleButton(dbg) {
  const callStackBody = findElement(dbg, "callStackBody");
  return callStackBody.querySelector(".show-more");
}

// Create an HTTP server to simulate an angular app with anonymous functions
// and return the URL.
function createMockAngularPage() {
  const httpServer = createTestHTTPServer();

  httpServer.registerContentType("html", "text/html");
  httpServer.registerContentType("js", "application/javascript");

  const htmlFilename = "angular-mock.html";
  httpServer.registerPathHandler(
    `/${htmlFilename}`,
    function (request, response) {
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.write(`
        <html>
            <button class="pause">Click me</button>
            <script type="text/javascript" src="angular.js"></script>
        </html>`);
    }
  );

  // Register an angular.js file in order to create a Group with anonymous functions in
  // the callstack panel.
  httpServer.registerPathHandler("/angular.js", function (request, response) {
    response.setHeader("Content-Type", "application/javascript");
    response.write(`
      document.querySelector("button.pause").addEventListener("click", () => {
        (function() {
          debugger;
        })();
      })
    `);
  });

  const port = httpServer.identity.primaryPort;
  return `http://localhost:${port}/${htmlFilename}`;
}
