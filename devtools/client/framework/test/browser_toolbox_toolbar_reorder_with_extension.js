/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for reordering with an extension installed.

const { Toolbox } = require("devtools/client/framework/toolbox");

const EXTENSION = "@reorder.test";

const TEST_STARTING_ORDER = [
  "inspector",
  "webconsole",
  "jsdebugger",
  "styleeditor",
  "performance",
  "memory",
  "netmonitor",
  "storage",
  "accessibility",
  EXTENSION,
];

add_task(async function() {
  const extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      devtools_page: "extension.html",
      applications: {
        gecko: { id: EXTENSION },
      },
    },
    files: {
      "extension.html": `<!DOCTYPE html>
                         <html>
                           <head>
                             <meta charset="utf-8">
                           </head>
                           <body>
                             <script src="extension.js"></script>
                           </body>
                         </html>`,
      "extension.js": async () => {
        // eslint-disable-next-line
        await browser.devtools.panels.create("extension", "fake-icon.png", "empty.html");
        // eslint-disable-next-line
        browser.test.sendMessage("devtools-page-ready");
      },
      "empty.html": "",
    },
  });

  await extension.startup();

  const tab = await addTab("about:blank");
  const toolbox = await openToolboxForTab(
    tab,
    "webconsole",
    Toolbox.HostType.BOTTOM
  );
  await extension.awaitMessage("devtools-page-ready");

  const originalPreference = Services.prefs.getCharPref(
    "devtools.toolbox.tabsOrder"
  );
  const win = getWindow(toolbox);
  const {
    outerWidth: originalWindowWidth,
    outerHeight: originalWindowHeight,
  } = win;
  registerCleanupFunction(() => {
    Services.prefs.setCharPref(
      "devtools.toolbox.tabsOrder",
      originalPreference
    );
    win.resizeTo(originalWindowWidth, originalWindowHeight);
  });

  info("Test for DragAndDrop the extension tab");
  let dragTarget = EXTENSION;
  let dropTarget = "webconsole";
  let expectedOrder = [
    "inspector",
    EXTENSION,
    "webconsole",
    "jsdebugger",
    "styleeditor",
    "performance",
    "memory",
    "netmonitor",
    "storage",
    "accessibility",
  ];
  prepareToolTabReorderTest(toolbox, TEST_STARTING_ORDER);
  await dndToolTab(toolbox, dragTarget, dropTarget);
  assertToolTabOrder(toolbox, expectedOrder);
  assertToolTabSelected(toolbox, dragTarget);
  assertToolTabPreferenceOrder(expectedOrder);

  info("Test the case of that the extension tab is overflowed");
  prepareToolTabReorderTest(toolbox, TEST_STARTING_ORDER);
  await resizeWindow(toolbox, 800);
  await toolbox.selectTool("storage");
  dragTarget = "storage";
  dropTarget = "inspector";
  expectedOrder = [
    "storage",
    "inspector",
    "webconsole",
    "jsdebugger",
    "styleeditor",
    "performance",
    "memory",
    "netmonitor",
    "accessibility",
    EXTENSION,
  ];
  await dndToolTab(toolbox, dragTarget, dropTarget);
  assertToolTabPreferenceOrder(expectedOrder);
  await resizeWindow(toolbox, originalWindowWidth, originalWindowHeight);

  info("Test the preference after uninstalling extension");
  prepareToolTabReorderTest(toolbox, TEST_STARTING_ORDER);
  await extension.unload();
  dragTarget = "webconsole";
  dropTarget = "inspector";
  expectedOrder = [
    "webconsole",
    "inspector",
    "jsdebugger",
    "styleeditor",
    "performance",
    "memory",
    "netmonitor",
    "storage",
    "accessibility",
  ];
  await dndToolTab(toolbox, dragTarget, dropTarget);
  assertToolTabPreferenceOrder(expectedOrder);
});
