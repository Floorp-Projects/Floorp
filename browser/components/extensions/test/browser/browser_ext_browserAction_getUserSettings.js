"use strict";

async function background(test) {
  let resolvers = {};
  let tests = {};

  browser.test.onMessage.addListener(id => {
    let resolver = resolvers[id];
    browser.test.assertTrue(resolver, `Found resolver for ${id}`);
    browser.test.assertTrue(resolver.resolve, `${id} was not resolved yet`);
    resolver.resolve(id);
    resolver.resolve = null; // resolve can be used only once.
  });

  async function pinToToolbar(shouldPinToToolbar) {
    let identifier = `${
      shouldPinToToolbar ? "pin-to-toolbar" : "unpin-from-toolbar"
    }-${Object.keys(resolvers).length}`;
    resolvers[identifier] = {};
    resolvers[identifier].promise = new Promise(
      _resolve => (resolvers[identifier].resolve = _resolve)
    );
    browser.test.sendMessage("pinToToolbar", {
      identifier,
      shouldPinToToolbar,
    });
    await resolvers[identifier].promise;
  }

  let { manifest_version } = await browser.runtime.getManifest();
  let action = browser.browserAction;

  if (manifest_version === 3) {
    action = browser.action;
  }

  tests.getUserSettings = async function () {
    let userSettings = await action.getUserSettings();

    await pinToToolbar(true);
    userSettings = await action.getUserSettings();
    browser.test.assertTrue(
      userSettings.isOnToolbar,
      "isOnToolbar should be true after pinning to toolbar"
    );

    await pinToToolbar(false);
    userSettings = await action.getUserSettings();
    browser.test.assertFalse(
      userSettings.isOnToolbar,
      "isOnToolbar should be false after unpinning"
    );

    await pinToToolbar(true);
    userSettings = await action.getUserSettings();
    browser.test.assertTrue(
      userSettings.isOnToolbar,
      "isOnToolbar should be true after repinning"
    );

    await pinToToolbar(false);
    userSettings = await action.getUserSettings();
    browser.test.assertFalse(
      userSettings.isOnToolbar,
      "isOnToolbar should be false after unpinning"
    );

    await browser.test.notifyPass("getUserSettings");
  };

  tests.default_area_getUserSettings = async function () {
    let userSettings = await action.getUserSettings();

    browser.test.assertTrue(
      userSettings.isOnToolbar,
      "isOnToolbar should be true when one of ['navbar', 'tabstrip', 'personaltoolbar'] default_area is specified in manifest.json"
    );

    await pinToToolbar(false);
    userSettings = await action.getUserSettings();
    browser.test.assertFalse(
      userSettings.isOnToolbar,
      "isOnToolbar should be false after unpinning"
    );

    await browser.test.notifyPass("getUserSettings");
  };

  tests.menupanel_default_area_getUserSettings = async function () {
    let userSettings = await action.getUserSettings();

    browser.test.assertFalse(
      userSettings.isOnToolbar,
      "isOnToolbar should be false when default_area is 'menupanel' in manifest.json"
    );

    await pinToToolbar(true);
    userSettings = await action.getUserSettings();
    browser.test.assertTrue(
      userSettings.isOnToolbar,
      "isOnToolbar should be true after pinning"
    );

    await browser.test.notifyPass("getUserSettings");
  };

  tests[test]();
}

function pinToToolbar(shouldPinToToolbar, extension) {
  let newArea = shouldPinToToolbar
    ? CustomizableUI.AREA_NAVBAR
    : CustomizableUI.AREA_ADDONS;
  let newPosition = shouldPinToToolbar ? undefined : 0;
  let widget = getBrowserActionWidget(extension);
  CustomizableUI.addWidgetToArea(widget.id, newArea, newPosition);
}

add_task(async function browserAction_getUserSettings() {
  let manifest = {
    manifest: {
      manifest_version: 2,
      browser_action: {},
    },
    background: `(${background})('getUserSettings')`,
  };
  let extension = ExtensionTestUtils.loadExtension(manifest);
  extension.onMessage("pinToToolbar", ({ identifier, shouldPinToToolbar }) => {
    pinToToolbar(shouldPinToToolbar, extension);
    extension.sendMessage(identifier);
  });
  await extension.startup();
  await extension.awaitFinish("getUserSettings");
  await extension.unload();
});

add_task(async function action_getUserSettings() {
  let manifest = {
    manifest: {
      manifest_version: 3,
      action: {},
    },
    background: `(${background})('getUserSettings')`,
  };

  let extension = ExtensionTestUtils.loadExtension(manifest);
  extension.onMessage("pinToToolbar", ({ identifier, shouldPinToToolbar }) => {
    pinToToolbar(shouldPinToToolbar, extension);
    extension.sendMessage(identifier);
  });
  await extension.startup();
  await extension.awaitFinish("getUserSettings");
  await extension.unload();
});

add_task(async function browserAction_getUserSettings_default_area() {
  for (let default_area of ["navbar", "tabstrip", "personaltoolbar"]) {
    let manifest = {
      manifest: {
        manifest_version: 2,
        browser_action: {
          default_area,
        },
      },
      background: `(${background})('default_area_getUserSettings')`,
    };
    let extension = ExtensionTestUtils.loadExtension(manifest);
    extension.onMessage(
      "pinToToolbar",
      ({ identifier, shouldPinToToolbar }) => {
        pinToToolbar(shouldPinToToolbar, extension);
        extension.sendMessage(identifier);
      }
    );
    await extension.startup();
    await extension.awaitFinish("getUserSettings");
    await extension.unload();
  }
});

add_task(async function action_getUserSettings_default_area() {
  for (let default_area of ["navbar", "tabstrip", "personaltoolbar"]) {
    let manifest = {
      manifest: {
        manifest_version: 3,
        action: {
          default_area,
        },
      },
      background: `(${background})('default_area_getUserSettings')`,
    };
    let extension = ExtensionTestUtils.loadExtension(manifest);
    extension.onMessage(
      "pinToToolbar",
      ({ identifier, shouldPinToToolbar }) => {
        pinToToolbar(shouldPinToToolbar, extension);
        extension.sendMessage(identifier);
      }
    );
    await extension.startup();
    await extension.awaitFinish("getUserSettings");
    await extension.unload();
  }
});

add_task(async function browserAction_getUserSettings_menupanel_default_area() {
  let manifest = {
    manifest: {
      manifest_version: 2,
      browser_action: {
        default_area: "menupanel",
      },
    },
    background: `(${background})('menupanel_default_area_getUserSettings')`,
  };
  let extension = ExtensionTestUtils.loadExtension(manifest);
  extension.onMessage("pinToToolbar", ({ identifier, shouldPinToToolbar }) => {
    pinToToolbar(shouldPinToToolbar, extension);
    extension.sendMessage(identifier);
  });
  await extension.startup();
  await extension.awaitFinish("getUserSettings");
  await extension.unload();
});

add_task(async function action_getUserSettings_menupanel_default_area() {
  let manifest = {
    manifest: {
      manifest_version: 3,
      action: {
        default_area: "menupanel",
      },
    },
    background: `(${background})('menupanel_default_area_getUserSettings')`,
  };
  let extension = ExtensionTestUtils.loadExtension(manifest);
  extension.onMessage("pinToToolbar", ({ identifier, shouldPinToToolbar }) => {
    pinToToolbar(shouldPinToToolbar, extension);
    extension.sendMessage(identifier);
  });
  await extension.startup();
  await extension.awaitFinish("getUserSettings");
  await extension.unload();
});
