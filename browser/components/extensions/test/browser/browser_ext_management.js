"use strict";

const BASE = "http://mochi.test:8888/browser/browser/components/extensions/test/browser/";

add_task(async function test_management_install() {
  await SpecialPowers.pushPrefEnv({set: [
    ["xpinstall.signatures.required", false],
  ]});

  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
  });

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {
        "browser_style": false,
      },
      permissions: ["management"],
    },
    background() {
      let addons;
      browser.test.onMessage.addListener((msg, init) => {
        addons = init;
        browser.test.sendMessage("ready");
      });
      browser.browserAction.onClicked.addListener(async () => {
        try {
          let {url, hash} = addons.shift();
          browser.test.log(`Installing XPI from ${url} with hash ${hash || "missing"}`);
          let {id} = await browser.management.install({url, hash});
          let {type} = await browser.management.get(id);
          browser.test.sendMessage("installed", {id, type});
        } catch (e) {
          browser.test.log(`management.install() throws ${e}`);
          browser.test.sendMessage("failed", e.message);
        }
      });
    },
  });

  let addons = [{
    url: BASE + "install_theme-1.0-fx.xpi",
    hash: "sha256:aa232d8391d82a9c1014364efbe1657ff6d8dfc88b3c71e99881b1f3843fdad3",
  }, {
    url: BASE + "install_other-1.0-fx.xpi",
  }];

  await extension.startup();
  extension.sendMessage("addons", addons);
  await extension.awaitMessage("ready");

  // Test installing a static WE theme.
  clickBrowserAction(extension);

  let {id, type} = await extension.awaitMessage("installed");
  is(id, "tiger@persona.beard", "Static web extension theme installed");
  is(type, "theme", "Extension type is correct");

  let style = window.getComputedStyle(document.documentElement);
  is(style.backgroundColor, "rgb(255, 165, 0)", "Background is the new black");

  let addon = await AddonManager.getAddonByID("tiger@persona.beard");

  Assert.deepEqual(addon.installTelemetryInfo, {
    source: "extension",
    method: "management-webext-api",
  }, "Got the expected telemetry info on the installed webext theme");

  await addon.uninstall();

  // Test installing a standard WE.
  clickBrowserAction(extension);
  let error = await extension.awaitMessage("failed");
  is(error, "Incompatible addon", "Standard web extension rejected");

  await extension.unload();
});
