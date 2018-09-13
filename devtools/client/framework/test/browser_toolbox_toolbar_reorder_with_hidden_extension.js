/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for reordering with an hidden extension installed.

const { Toolbox } = require("devtools/client/framework/toolbox");

const EXTENSION = "@reorder.test";

const TEST_DATA = [
  {
    description: "Test that drags a tab to left beyond the extension's tab",
    startingOrder: ["inspector", EXTENSION, "webconsole", "jsdebugger", "styleeditor",
                    "performance", "memory", "netmonitor", "storage", "accessibility"],
    dragTarget: "webconsole",
    dropTarget: "inspector",
    expectedOrder: ["webconsole", "inspector", EXTENSION, "jsdebugger", "styleeditor",
                    "performance", "memory", "netmonitor", "storage", "accessibility"],
  },
  {
    description: "Test that drags a tab to right beyond the extension's tab",
    startingOrder: ["inspector", EXTENSION, "webconsole", "jsdebugger", "styleeditor",
                    "performance", "memory", "netmonitor", "storage", "accessibility"],
    dragTarget: "inspector",
    dropTarget: "webconsole",
    expectedOrder: [EXTENSION, "webconsole", "inspector", "jsdebugger", "styleeditor",
                    "performance", "memory", "netmonitor", "storage", "accessibility"],
  },
  {
    description: "Test that drags a tab to left end, but hidden tab is left end",
    startingOrder: [EXTENSION, "inspector", "webconsole", "jsdebugger", "styleeditor",
                    "performance", "memory", "netmonitor", "storage", "accessibility"],
    dragTarget: "webconsole",
    dropTarget: "inspector",
    expectedOrder: [EXTENSION, "webconsole", "inspector", "jsdebugger", "styleeditor",
                    "performance", "memory", "netmonitor", "storage", "accessibility"],
  },
  {
    description: "Test that drags a tab to right end, but hidden tab is right end",
    startingOrder: ["inspector", "webconsole", "jsdebugger", "styleeditor", "performance",
                    "memory", "netmonitor", "storage", "accessibility", EXTENSION],
    dragTarget: "webconsole",
    dropTarget: "accessibility",
    expectedOrder: ["inspector", "jsdebugger", "styleeditor", "performance", "memory",
                    "netmonitor", "storage", "accessibility", EXTENSION, "webconsole"],
  },
];

add_task(async function() {
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("devtools.toolbox.tabsOrder");
  });

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
        // Don't call browser.devtools.panels.create since this need to be as hidden.
        // eslint-disable-next-line
        browser.test.sendMessage("devtools-page-ready");
      },
    },
  });

  await extension.startup();

  let tab = await addTab("about:blank");
  const toolbox = await openToolboxForTab(tab, "webconsole", Toolbox.HostType.BOTTOM);
  await extension.awaitMessage("devtools-page-ready");

  for (const { description, startingOrder,
               dragTarget, dropTarget, expectedOrder } of TEST_DATA) {
    info(description);
    prepareTestWithHiddenExtension(toolbox, startingOrder);
    await dndToolTab(toolbox, dragTarget, dropTarget);
    assertToolTabPreferenceOrder(expectedOrder);
  }

  info("Check ordering preference after destroying toolbox");
  let target = gDevTools.getTargetForTab(tab);
  await gDevTools.closeToolbox(target);
  await target.destroy();
  assertExtensionExistence(true);

  info("Check ordering preference after uninstalling hidden addon");
  await extension.unload();
  tab = await addTab("about:blank");
  await openToolboxForTab(tab, "webconsole", Toolbox.HostType.BOTTOM);
  target = gDevTools.getTargetForTab(tab);
  await gDevTools.closeToolbox(target);
  assertExtensionExistence(false);
});

function assertExtensionExistence(shouldExist) {
  const ids = Services.prefs.getCharPref("devtools.toolbox.tabsOrder").split(",");
  is(ids.includes(EXTENSION), shouldExist,
     `Hidden extension id should ${shouldExist ? "" : "not "}exist`);
}

function prepareTestWithHiddenExtension(toolbox, startingOrder) {
  Services.prefs.setCharPref("devtools.toolbox.tabsOrder", startingOrder.join(","));

  for (const id of startingOrder) {
    if (id === EXTENSION) {
      ok(!getElementByToolId(toolbox, id), "Hidden extension tab should not exist");
    } else {
      ok(getElementByToolId(toolbox, id), `Tab element should exist for ${ id }`);
    }
  }
}
