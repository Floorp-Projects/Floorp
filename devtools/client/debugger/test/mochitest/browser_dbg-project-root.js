/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// This test covers changing to a distinct "project root"
// i.e. displaying only one particular thread, domain, or directory in the Source Tree.

"use strict";

const httpServer = createTestHTTPServer();
const HOST = `localhost:${httpServer.identity.primaryPort}`;
const BASE_URL = `http://${HOST}/`;

const INDEX_PAGE_CONTENT = `<!DOCTYPE html>
  <html>
    <head>
      <script type="text/javascript" src="/root-script.js"></script>
      <script type="text/javascript" src="/folder/folder-script.js"></script>
      <script type="text/javascript" src="/folder/sub-folder/sub-folder-script.js"></script>
    </head>
    <body></body>
  </html>`;

httpServer.registerPathHandler("/index.html", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write(INDEX_PAGE_CONTENT);
});
httpServer.registerPathHandler("/root-script.js", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write("console.log('root script')");
});
httpServer.registerPathHandler(
  "/folder/folder-script.js",
  (request, response) => {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write("console.log('folder script')");
  }
);
httpServer.registerPathHandler(
  "/folder/sub-folder/sub-folder-script.js",
  (request, response) => {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write("console.log('sub folder script')");
  }
);

const ALL_SCRIPTS = [
  "root-script.js",
  "folder-script.js",
  "sub-folder-script.js",
];

add_task(async function testProjectRoot() {
  const dbg = await initDebuggerWithAbsoluteURL(
    BASE_URL + "index.html",
    ...ALL_SCRIPTS
  );

  await waitForSourcesInSourceTree(dbg, ALL_SCRIPTS);

  info("Select the Main Thread as project root");
  const threadItem = findSourceNodeWithText(dbg, "Main Thread");
  await setProjectRoot(dbg, threadItem);
  assertRootLabel(dbg, "Main Thread");
  await waitForSourcesInSourceTree(dbg, ALL_SCRIPTS);

  info("Select the host as project root");
  const hostItem = findSourceNodeWithText(dbg, HOST);
  await setProjectRoot(dbg, hostItem);
  assertRootLabel(dbg, HOST);
  await waitForSourcesInSourceTree(dbg, ALL_SCRIPTS);

  info("Select 'folder' as project root");
  const folderItem = findSourceNodeWithText(dbg, "folder");
  await setProjectRoot(dbg, folderItem);
  assertRootLabel(dbg, "folder");
  await waitForSourcesInSourceTree(dbg, [
    "folder-script.js",
    "sub-folder-script.js",
  ]);

  info("Reload and see if project root is preserved");
  await reload(dbg, "folder-script.js", "sub-folder-script.js");
  assertRootLabel(dbg, "folder");
  await waitForSourcesInSourceTree(dbg, [
    "folder-script.js",
    "sub-folder-script.js",
  ]);

  info("Select 'sub-folder' as project root");
  const subFolderItem = findSourceNodeWithText(dbg, "sub-folder");
  await setProjectRoot(dbg, subFolderItem);
  assertRootLabel(dbg, "sub-folder");
  await waitForSourcesInSourceTree(dbg, ["sub-folder-script.js"]);

  info("Clear project root");
  await clearProjectRoot(dbg);
  await waitForSourcesInSourceTree(dbg, ALL_SCRIPTS);
});

async function setProjectRoot(dbg, treeNode) {
  const onContextMenu = waitForContextMenu(dbg);
  rightClickEl(dbg, treeNode);
  const menupopup = await onContextMenu;
  const onHidden = new Promise(resolve => {
    menupopup.addEventListener("popuphidden", resolve, { once: true });
  });
  selectContextMenuItem(dbg, "#node-set-directory-root");
  await onHidden;
}

function assertRootLabel(dbg, label) {
  const rootHeaderLabel = dbg.win.document.querySelector(
    ".sources-clear-root-label"
  );
  is(rootHeaderLabel.textContent, label);
}

async function clearProjectRoot(dbg) {
  const rootHeader = dbg.win.document.querySelector(".sources-clear-root");
  rootHeader.click();
}
