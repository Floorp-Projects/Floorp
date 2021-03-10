/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DevToolsClient } = require("devtools/client/devtools-client");
const { DevToolsServer } = require("devtools/server/devtools-server");

add_task(async function test_webextension_target_allowSource_on_eager_eval() {
  const extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    background: function() {
      this.browser.test.sendMessage("bg-ready");
    },
  });

  await extension.startup();
  await extension.awaitMessage("bg-ready");

  // Init debugger server.
  DevToolsServer.init();
  DevToolsServer.registerAllActors();

  // Create and connect a debugger client.
  const transport = DevToolsServer.connectPipe();
  const client = new DevToolsClient(transport);
  await client.connect();

  // Get AddonTarget.
  const addonDescriptor = await client.mainRoot.getAddon({ id: extension.id });
  ok(addonDescriptor, "webextension addon description has been found");

  // Open a toolbox window for the addon target.
  const toolbox = await gDevTools.showToolbox(
    addonDescriptor,
    "webconsole",
    Toolbox.HostType.WINDOW
  );

  await toolbox.selectTool("webconsole");

  info("Start listening for console messages");
  SpecialPowers.registerConsoleListener(msg => {
    if (
      msg.message &&
      msg.message.includes("Unexpected invalid url: debugger eager eval code")
    ) {
      ok(
        false,
        "webextension targetActor._allowSource should not log an error on debugger eager eval code"
      );
    }
  });
  registerCleanupFunction(() => {
    SpecialPowers.postConsoleSentinel();
  });

  const hud = toolbox.getPanel("webconsole").hud;
  setInputValue(hud, `browser`);

  info("Wait for eager eval element");
  await TestUtils.waitForCondition(() => getEagerEvaluationElement(hud));

  // The following step will force Firefox to list the source actors, one of those
  // source actors is going to be the one related to the js code evaluated by the
  // eager evaluation and it does make sure that WebExtensionTargetPrototype._allowSource
  // is going to be called with the source actor with url "debugger eager eval code".
  info("Select the debugger panel to force webextension actor to list sources");
  await toolbox.selectTool("jsdebugger");

  // Wait for a bit so that the error message would have the time to be logged
  // (and fail if the issue does regress again).
  await wait(2000);

  await toolbox.destroy();
  await client.close();

  await extension.unload();
});
