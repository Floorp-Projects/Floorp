/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  DevToolsServer,
} = require("resource://devtools/server/devtools-server.js");

// Bug 1277805: Too slow for debug runs
requestLongerTimeout(2);

/**
 * Bug 979536: Ensure fronts are destroyed after toolbox close.
 *
 * The fronts need to be destroyed manually to unbind their onPacket handlers.
 *
 * When you initialize a front and call |this.manage|, it adds a client actor
 * pool that the DevToolsClient uses to route packet replies to that actor.
 *
 * Most (all?) tools create a new front when they are opened.  When the destroy
 * step is skipped and the tool is reopened, a second front is created and also
 * added to the client actor pool.  When a packet reply is received, is ends up
 * being routed to the first (now unwanted) front that is still in the client
 * actor pool.  Since this is not the same front that was used to make the
 * request, an error occurs.
 *
 * This problem does not occur with the toolbox for a local tab because the
 * toolbox target creates its own DevToolsClient for the local tab, and the
 * client is destroyed when the toolbox is closed, which removes the client
 * actor pools, and avoids this issue.
 *
 * In remote debugging, we do not destroy the DevToolsClient on toolbox close
 * because it can still used for other targets.
 * Thus, the same client gets reused across multiple toolboxes,
 * which leads to the tools failing if they don't destroy their fronts.
 */

function runTools(tab) {
  return (async function () {
    let toolbox;
    const toolIds = await getSupportedToolIds(tab);
    for (const toolId of toolIds) {
      info("About to open " + toolId);
      toolbox = await gDevTools.showToolboxForTab(tab, {
        toolId,
        hostType: "window",
      });
      ok(toolbox, "toolbox exists for " + toolId);
      is(toolbox.currentToolId, toolId, "currentToolId should be " + toolId);

      const panel = toolbox.getCurrentPanel();
      ok(panel, toolId + " panel has been registered in the toolbox");
    }

    const client = toolbox.commands.client;
    await toolbox.destroy();

    // We need to check the client after the toolbox destruction.
    return client;
  })();
}

function test() {
  (async function () {
    toggleAllTools(true);
    const tab = await addTab("about:blank");

    const client = await runTools(tab);

    const rootFronts = [...client.mainRoot.fronts.values()];

    // Actor fronts should be destroyed now that the toolbox has closed, but
    // look for any that remain.
    for (const pool of client.__pools) {
      if (!pool.__poolMap) {
        continue;
      }

      // Ignore the root fronts, which are top-level pools and aren't released
      // on toolbox destroy, but on client close.
      if (rootFronts.includes(pool)) {
        continue;
      }

      for (const actor of pool.__poolMap.keys()) {
        // Ignore the root front as it is only release on client close
        if (actor == "root") {
          continue;
        }
        ok(false, "Front for " + actor + " still held in pool!");
      }
    }

    gBrowser.removeCurrentTab();
    DevToolsServer.destroy();
    toggleAllTools(false);
    finish();
  })();
}
