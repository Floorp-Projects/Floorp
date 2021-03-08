/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DevToolsClient } = require("devtools/client/devtools-client");
const { DevToolsServer } = require("devtools/server/devtools-server");

add_task(async function test_webextension_descriptors() {
  const extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      name: "Descriptor extension",
    },
  });

  await extension.startup();

  // Init debugger server.
  DevToolsServer.init();
  DevToolsServer.registerAllActors();

  // Create and connect a debugger client.
  const transport = DevToolsServer.connectPipe();
  const client = new DevToolsClient(transport);
  await client.connect();

  // Get AddonTarget.
  const descriptor = await client.mainRoot.getAddon({ id: extension.id });
  ok(descriptor, "webextension descriptor has been found");
  is(descriptor.name, "Descriptor extension", "Descriptor name is correct");
  is(descriptor.debuggable, true, "Descriptor debuggable attribute is correct");

  const onDestroyed = descriptor.once("descriptor-destroyed");
  info("Uninstall the extension");
  await extension.unload();
  info("Wait for the descriptor to be destroyed");
  await onDestroyed;

  await client.close();
});
