/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// The `AddonsManager` test helper can only be called once per test script.
// This `setup` task will run first.
add_task(async function setup() {
  Services.prefs.setBoolPref("extensions.blocklist.enabled", false);
  await startupAddonsManager();
});

// Basic request wrapper that sends a request and resolves on the next packet.
// Will only work for very basic scenarios, without events emitted on the server
// etc...
async function sendRequest(transport, request) {
  return new Promise(resolve => {
    transport.hooks = {
      onPacket: packet => {
        dump(`received packet: ${JSON.stringify(packet)}\n`);
        // Let's resolve only when we get a packet that is related to our
        // request. It is needed because some methods do not return the correct
        // response right away. This is the case of the `reload` method, which
        // receives a `addonListChanged` message first and then a `reload`
        // message.
        if (packet.from === request.to) {
          resolve(packet);
        }
      },
    };
    transport.send(request);
  });
}

// If this test case fails, please reach out to webext peers because
// https://github.com/mozilla/web-ext relies on the APIs tested here.
add_task(async function test_webext_run_apis() {
  DevToolsServer.init();
  DevToolsServer.registerAllActors();

  const transport = DevToolsServer.connectPipe();

  // After calling connectPipe, the root actor will be created on the server
  // and a packet will be emitted after a tick. Wait for the initial packet.
  await new Promise(resolve => {
    transport.hooks = { onPacket: resolve };
  });

  const getRootResponse = await sendRequest(transport, {
    to: "root",
    type: "getRoot",
  });

  ok(getRootResponse, "received a response after calling RootActor::getRoot");
  ok(getRootResponse.addonsActor, "getRoot returned an addonsActor id");

  // installTemporaryAddon
  const addonId = "test-addons-actor@mozilla.org";
  const addonPath = getFilePath("addons/web-extension", false, true);
  const promiseStarted = AddonTestUtils.promiseWebExtensionStartup(addonId);
  const { addon } = await sendRequest(transport, {
    to: getRootResponse.addonsActor,
    type: "installTemporaryAddon",
    addonPath,
    // The openDevTools parameter is not always passed by web-ext. This test
    // omits it, to make sure that the request without the flag is accepted.
    // openDevTools: false,
  });
  await promiseStarted;

  ok(addon, "addonsActor allows to install a temporary add-on");
  equal(addon.id, addonId, "temporary add-on is the expected one");
  equal(addon.actor, false, "temporary add-on does not have an actor");

  // listAddons
  const { addons } = await sendRequest(transport, {
    to: "root",
    type: "listAddons",
  });
  ok(Array.isArray(addons), "listAddons() returns a list of add-ons");

  const installedAddon = addons[0];
  equal(installedAddon.id, addonId, "installed add-on is the expected one");
  ok(installedAddon.actor, "returned add-on has an actor");

  // reload
  const promiseReloaded = AddonTestUtils.promiseAddonEvent("onInstalled");
  const promiseRestarted = AddonTestUtils.promiseWebExtensionStartup(addonId);
  await sendRequest(transport, {
    to: installedAddon.actor,
    type: "reload",
  });
  await Promise.all([promiseReloaded, promiseRestarted]);

  transport.close();
});
