/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function test_webextension_descriptors() {
  const extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      name: "Descriptor extension",
    },
  });

  await extension.startup();

  // Get AddonTarget.
  const commands = await CommandsFactory.forAddon(extension.id);
  const descriptor = commands.descriptorFront;
  ok(descriptor, "webextension descriptor has been found");
  is(descriptor.name, "Descriptor extension", "Descriptor name is correct");
  is(descriptor.debuggable, true, "Descriptor debuggable attribute is correct");

  const onDestroyed = descriptor.once("descriptor-destroyed");
  info("Uninstall the extension");
  await extension.unload();
  info("Wait for the descriptor to be destroyed");
  await onDestroyed;

  await commands.destroy();
});
