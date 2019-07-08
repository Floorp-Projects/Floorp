/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {
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
    },
  });

  SimpleTest.waitForExplicitFinish();
  let waitForConsole = new Promise(resolve => {
    SimpleTest.monitorConsole(resolve, [
      {
        message: /Reading manifest: Error processing browser_action.unrecognized_property: An unexpected property was found/,
      },
    ]);
  });

  await extension.startup();

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
});
