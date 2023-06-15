/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Testing the script overrides feature
 */

"use strict";

const httpServer = createTestHTTPServer();
const BASE_URL = `http://localhost:${httpServer.identity.primaryPort}/`;

httpServer.registerContentType("html", "text/html");
httpServer.registerContentType("js", "application/javascript");

const testSourceContent = `function foo() {
  console.log("original test script");
}`;

const testOverrideSourceContent = `function foo() {
  console.log("overriden test script");
}
foo();
`;

httpServer.registerPathHandler("/index.html", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write(`<!DOCTYPE html>
  <html>
    <head>
    <script type="text/javascript" src="/test.js"></script>
    </head>
    <body>
    </body>
  </html>
  `);
});

httpServer.registerPathHandler("/test.js", (request, response) => {
  response.setHeader("Content-Type", "application/javascript");
  response.write(testSourceContent);
});

add_task(async function () {
  const dbg = await initDebuggerWithAbsoluteURL(
    BASE_URL + "index.html",
    "test.js"
  );

  await waitForSourcesInSourceTree(dbg, ["test.js"], {
    noExpand: false,
  });

  info("Load and assert the content of the test.js script");
  await selectSourceFromSourceTree(
    dbg,
    "test.js",
    3,
    "Select the `test.js` script for the tree"
  );
  is(
    getCM(dbg).getValue(),
    testSourceContent,
    "The test.js is the original source content"
  );

  info("Add a breakpoint");
  await addBreakpoint(dbg, "test.js", 2);

  info("Select test.js tree node, and add override");
  const MockFilePicker = SpecialPowers.MockFilePicker;
  MockFilePicker.init(window);
  const nsiFile = new FileUtils.File(
    PathUtils.join(PathUtils.tempDir, "test.js")
  );
  MockFilePicker.setFiles([nsiFile]);
  const path = nsiFile.path;

  await triggerSourceTreeContextMenu(
    dbg,
    findSourceNodeWithText(dbg, "test.js"),
    "#node-menu-overrides"
  );

  info("Wait for `test.js` to be saved to disk and re-write it");
  await BrowserTestUtils.waitForCondition(() => IOUtils.exists(path));
  await BrowserTestUtils.waitForCondition(async () => {
    const { size } = await IOUtils.stat(path);
    return size > 0;
  });

  await IOUtils.write(
    path,
    new TextEncoder().encode(testOverrideSourceContent)
  );

  info("Reload and assert `test.js` pause and it overriden source content");
  const onReloaded = reload(dbg, "test.js");
  await waitForPaused(dbg);

  assertPausedAtSourceAndLine(dbg, findSource(dbg, "test.js").id, 2);
  is(
    getCM(dbg).getValue(),
    testOverrideSourceContent,
    "The test.js is the overridden source content"
  );

  await resume(dbg);
  await onReloaded;

  info("Remove override and test");
  const removed = waitForDispatch(dbg.store, "REMOVE_OVERRIDE");
  await triggerSourceTreeContextMenu(
    dbg,
    findSourceNodeWithText(dbg, "test.js"),
    "#node-menu-overrides"
  );
  await removed;

  info("Reload and assert `test.js`'s original source content");
  await reload(dbg, "test.js");
  await waitForSelectedSource(dbg, "test.js");

  is(
    getCM(dbg).getValue(),
    testSourceContent,
    "The test.js is the original source content"
  );
});
