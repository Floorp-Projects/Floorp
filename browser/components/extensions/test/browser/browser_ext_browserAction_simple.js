/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

async function testAction(manifest_version) {
  const action = manifest_version < 3 ? "browser_action" : "action";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version,
      [action]: {
        default_popup: "popup.html",
        unrecognized_property: "with-a-random-value",
      },
      icons: { 32: "icon.png" },
    },

    files: {
      "popup.html": `
      <!DOCTYPE html>
      <html><body>
      <script src="popup.js"></script>
      </body></html>
      `,

      "popup.js": function() {
        window.onload = () => {
          browser.runtime.sendMessage("from-popup");
        };
      },
      "icon.png": imageBuffer,
    },

    background: function() {
      browser.runtime.onMessage.addListener(msg => {
        browser.test.assertEq(msg, "from-popup", "correct message received");
        browser.test.sendMessage("popup");
      });

      // Test what api namespace is valid, make sure both are not.
      let manifest = browser.runtime.getManifest();
      let { manifest_version } = manifest;
      browser.test.assertEq(
        manifest_version == 2,
        "browserAction" in browser,
        "browserAction is available"
      );
      browser.test.assertEq(
        manifest_version !== 2,
        "action" in browser,
        "action is available"
      );
    },
  });

  SimpleTest.waitForExplicitFinish();
  let waitForConsole = new Promise(resolve => {
    SimpleTest.monitorConsole(resolve, [
      {
        message: new RegExp(
          `Reading manifest: Warning processing ${action}.unrecognized_property: An unexpected property was found`
        ),
      },
    ]);
  });

  ExtensionTestUtils.failOnSchemaWarnings(false);
  await extension.startup();
  ExtensionTestUtils.failOnSchemaWarnings(true);

  // Do this a few times to make sure the pop-up is reloaded each time.
  for (let i = 0; i < 3; i++) {
    clickBrowserAction(extension);

    let widget = getBrowserActionWidget(extension).forWindow(window);
    let image = getComputedStyle(widget.node).listStyleImage;

    ok(image.includes("/icon.png"), "The extension's icon is used");
    await extension.awaitMessage("popup");

    closeBrowserAction(extension);
  }

  await extension.unload();

  SimpleTest.endMonitorConsole();
  await waitForConsole;
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.manifestV3.enabled", true]],
  });
});

add_task(async function test_browserAction() {
  await testAction(2);
});

add_task(async function test_action() {
  await testAction(3);
});
