/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function testDisabled() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {},
    },

    background: function() {
      let clicked = false;

      browser.browserAction.onClicked.addListener(() => {
        browser.test.log("Got click event");
        clicked = true;
      });

      browser.test.onMessage.addListener((msg, expectClick) => {
        if (msg == "enable") {
          browser.test.log("enable browserAction");
          browser.browserAction.enable();
        } else if (msg == "disable") {
          browser.test.log("disable browserAction");
          browser.browserAction.disable();
        } else if (msg == "check-clicked") {
          browser.test.assertEq(expectClick, clicked, "got click event?");
          clicked = false;
        } else {
          browser.test.fail("Unexpected message");
        }

        browser.test.sendMessage("next-test");
      });

      browser.test.sendMessage("ready");
    },
  });

  await extension.startup();
  await extension.awaitMessage("ready");

  await clickBrowserAction(extension);
  await new Promise(resolve => setTimeout(resolve, 0));

  extension.sendMessage("check-clicked", true);
  await extension.awaitMessage("next-test");

  extension.sendMessage("disable");
  await extension.awaitMessage("next-test");

  await clickBrowserAction(extension);
  await new Promise(resolve => setTimeout(resolve, 0));

  extension.sendMessage("check-clicked", false);
  await extension.awaitMessage("next-test");

  extension.sendMessage("enable");
  await extension.awaitMessage("next-test");

  await clickBrowserAction(extension);
  await new Promise(resolve => setTimeout(resolve, 0));

  extension.sendMessage("check-clicked", true);
  await extension.awaitMessage("next-test");

  await extension.unload();
});
