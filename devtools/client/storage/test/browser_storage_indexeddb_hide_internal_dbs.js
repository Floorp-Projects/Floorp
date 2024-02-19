/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/framework/browser-toolbox/test/helpers-browser-toolbox.js",
  this
);

// Test that internal DBs are hidden in the regular toolbox,but visible in the
// Browser Toolbox
add_task(async function () {
  await openTabAndSetupStorage(
    MAIN_DOMAIN_SECURED + "storage-empty-objectstores.html"
  );
  const doc = gPanelWindow.document;

  // check regular toolbox
  info("Check indexedDB tree in toolbox");
  const hosts = getDBHostsInTree(doc);
  is(hosts.length, 1, "There is only one host for indexedDB storage");
  is(hosts[0], "https://test1.example.org", "Host is test1.example.org");

  // check browser toolbox
  info("awaiting to open browser toolbox");
  const ToolboxTask = await initBrowserToolboxTask();
  await ToolboxTask.importFunctions({ getDBHostsInTree });

  await ToolboxTask.spawn(null, async () => {
    info("Selecting storage panel");
    await gToolbox.selectTool("storage");
    info("Check indexedDB tree in browser toolbox");
    const browserToolboxDoc = gToolbox.getCurrentPanel().panelWindow.document;

    const browserToolboxHosts = getDBHostsInTree(browserToolboxDoc);
    // In the spawn task, we don't have access to Assert:
    // eslint-disable-next-line mozilla/no-comparison-or-assignment-inside-ok
    ok(browserToolboxHosts.length > 1, "There are more than 1 indexedDB hosts");
    ok(
      browserToolboxHosts.includes("about:devtools-toolbox"),
      "about:devtools-toolbox host is present"
    );
    ok(browserToolboxHosts.includes("chrome"), "chrome host is present");
    ok(
      browserToolboxHosts.includes("indexeddb+++fx-devtools"),
      "fx-devtools host is present"
    );
  });

  info("Destroying browser toolbox");
  await ToolboxTask.destroy();
});

function getDBHostsInTree(doc) {
  const treeId = JSON.stringify(["indexedDB"]);
  const items = doc.querySelectorAll(
    `[data-id='${treeId}'] > .tree-widget-children > *`
  );

  // the host is located at the 2nd element of the array in data-id
  return [...items].map(x => JSON.parse(x.dataset.id)[1]);
}
