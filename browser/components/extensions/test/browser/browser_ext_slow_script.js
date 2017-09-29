/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_slow_content_script() {
  // Make sure we get a new process for our tab, or our reportProcessHangs
  // preferences value won't apply to it.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.ipc.processCount", 1],
      ["dom.ipc.keepProcessesAlive.web", 0],
    ],
  });
  await SpecialPowers.popPrefEnv();

  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.ipc.processCount", 8],
      ["dom.ipc.processPrelaunch.enabled", false],
      ["dom.ipc.reportProcessHangs", true],
    ],
  });

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",

    manifest: {
      name: "Slow Script Extension",

      content_scripts: [{
        matches: ["http://example.com/"],
        js: ["content.js"],
      }],
    },

    files: {
      "content.js": function() {
        while (true) {
          // Busy wait.
        }
      },
    },
  });

  await extension.startup();

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");

  let notification = await BrowserTestUtils.waitForGlobalNotificationBar(window, "process-hang");
  let text = document.getAnonymousElementByAttribute(notification, "anonid", "messageText").textContent;

  ok(text.includes("\u201cSlow Script Extension\u201d"),
     "Label is correct");

  let stopButton = notification.querySelector("[label='Stop It']");
  stopButton.click();

  await BrowserTestUtils.removeTab(tab);

  await extension.unload();
});
