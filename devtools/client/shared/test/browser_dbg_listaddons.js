/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper_addons.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/helper_addons.js",
  this
);

var { DevToolsServer } = require("devtools/server/devtools-server");
var { DevToolsClient } = require("devtools/client/devtools-client");

/**
 * Make sure the listAddons request works as specified.
 */
const ADDON1_ID = "test-addon-1@mozilla.org";
const ADDON1_PATH = "addons/test-addon-1/";
const ADDON2_ID = "test-addon-2@mozilla.org";
const ADDON2_PATH = "addons/test-addon-2/";

add_task(async function() {
  DevToolsServer.init();
  DevToolsServer.registerAllActors();

  const transport = DevToolsServer.connectPipe();
  const client = new DevToolsClient(transport);

  const [type] = await client.connect();
  is(type, "browser", "Root actor should identify itself as a browser.");

  let addonListChangedEvents = 0;
  client.mainRoot.on("addonListChanged", () => addonListChangedEvents++);

  const addon1 = await addTemporaryAddon(ADDON1_PATH);
  const addonFront1 = await client.mainRoot.getAddon({ id: ADDON1_ID });
  is(addonListChangedEvents, 0, "Should not receive addonListChanged yet.");
  ok(addonFront1, "Should find an addon actor for addon1.");

  const addon2 = await addTemporaryAddon(ADDON2_PATH);
  const front1AfterAddingAddon2 = await client.mainRoot.getAddon({
    id: ADDON1_ID,
  });
  const addonFront2 = await client.mainRoot.getAddon({ id: ADDON2_ID });

  is(
    addonListChangedEvents,
    1,
    "Should have received an addonListChanged event."
  );
  ok(addonFront2, "Should find an addon actor for addon2.");
  is(
    addonFront1,
    front1AfterAddingAddon2,
    "Front for addon1 should be the same"
  );

  await removeAddon(addon1);
  const front1AfterRemove = await client.mainRoot.getAddon({ id: ADDON1_ID });
  is(
    addonListChangedEvents,
    2,
    "Should have received an addonListChanged event."
  );
  ok(!front1AfterRemove, "Should no longer get a front for addon1");

  await removeAddon(addon2);
  const front2AfterRemove = await client.mainRoot.getAddon({ id: ADDON2_ID });
  is(
    addonListChangedEvents,
    3,
    "Should have received an addonListChanged event."
  );
  ok(!front2AfterRemove, "Should no longer get a front for addon1");

  await client.close();
});
