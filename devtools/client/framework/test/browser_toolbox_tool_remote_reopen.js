/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { DebuggerServer } = require("devtools/server/main");
const { DebuggerClient } = require("devtools/shared/client/debugger-client");

// Bug 1277805: Too slow for debug runs
requestLongerTimeout(2);

/**
 * Bug 979536: Ensure fronts are destroyed after toolbox close.
 *
 * The fronts need to be destroyed manually to unbind their onPacket handlers.
 *
 * When you initialize a front and call |this.manage|, it adds a client actor
 * pool that the DebuggerClient uses to route packet replies to that actor.
 *
 * Most (all?) tools create a new front when they are opened.  When the destroy
 * step is skipped and the tool is reopened, a second front is created and also
 * added to the client actor pool.  When a packet reply is received, is ends up
 * being routed to the first (now unwanted) front that is still in the client
 * actor pool.  Since this is not the same front that was used to make the
 * request, an error occurs.
 *
 * This problem does not occur with the toolbox for a local tab because the
 * toolbox target creates its own DebuggerClient for the local tab, and the
 * client is destroyed when the toolbox is closed, which removes the client
 * actor pools, and avoids this issue.
 *
 * In WebIDE, we do not destroy the DebuggerClient on toolbox close because it
 * is still used for other purposes like managing apps, etc. that aren't part of
 * a toolbox.  Thus, the same client gets reused across multiple toolboxes,
 * which leads to the tools failing if they don't destroy their fronts.
 */

function runTools(target) {
  return (async function() {
    const toolIds = gDevTools.getToolDefinitionArray()
                           .filter(def => def.isTargetSupported(target))
                           .map(def => def.id);

    let toolbox;
    for (let index = 0; index < toolIds.length; index++) {
      const toolId = toolIds[index];

      info("About to open " + index + "/" + toolId);
      toolbox = await gDevTools.showToolbox(target, toolId, "window");
      ok(toolbox, "toolbox exists for " + toolId);
      is(toolbox.currentToolId, toolId, "currentToolId should be " + toolId);

      const panel = toolbox.getCurrentPanel();
      ok(panel.isReady, toolId + " panel should be ready");
    }

    await toolbox.destroy();
  })();
}

function getClient() {
  DebuggerServer.init();
  DebuggerServer.registerAllActors();

  const transport = DebuggerServer.connectPipe();
  const client = new DebuggerClient(transport);

  return client.connect().then(() => client);
}

function getTarget(client) {
  const deferred = defer();

  client.listTabs().then(tabList => {
    const target = TargetFactory.forRemoteTab({
      client: client,
      form: tabList.tabs[tabList.selected],
      chrome: false
    });
    deferred.resolve(target);
  });

  return deferred.promise;
}

function test() {
  (async function() {
    toggleAllTools(true);
    await addTab("about:blank");

    const client = await getClient();
    const target = await getTarget(client);
    await runTools(target);

    // Actor fronts should be destroyed now that the toolbox has closed, but
    // look for any that remain.
    for (const pool of client.__pools) {
      if (!pool.__poolMap) {
        continue;
      }
      for (const actor of pool.__poolMap.keys()) {
        // Bug 1056342: Profiler fails today because of framerate actor, but
        // this appears more complex to rework, so leave it for that bug to
        // resolve.
        if (actor.includes("framerateActor")) {
          todo(false, "Front for " + actor + " still held in pool!");
          continue;
        }
        // gcliActor is for the commandline which is separate to the toolbox
        if (actor.includes("gcliActor")) {
          continue;
        }
        ok(false, "Front for " + actor + " still held in pool!");
      }
    }

    gBrowser.removeCurrentTab();
    DebuggerServer.destroy();
    toggleAllTools(false);
    finish();
  })();
}
