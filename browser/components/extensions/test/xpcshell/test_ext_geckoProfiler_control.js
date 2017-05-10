/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

let getExtension = () => {
  return ExtensionTestUtils.loadExtension({
    background: async function() {
      const runningListener = isRunning => {
        if (isRunning) {
          browser.test.sendMessage("started");
        } else {
          browser.test.sendMessage("stopped");
        }
      };

      browser.test.onMessage.addListener(async message => {
        let result;
        switch (message) {
          case "start":
            result = await browser.geckoProfiler.start({
              bufferSize: 10000,
              interval: 0.5,
              features: ["js"],
              threads: ["GeckoMain"],
            });
            browser.test.assertEq(undefined, result, "start returns nothing.");
            break;
          case "stop":
            result = await browser.geckoProfiler.stop();
            browser.test.assertEq(undefined, result, "stop returns nothing.");
            break;
          case "pause":
            result = await browser.geckoProfiler.pause();
            browser.test.assertEq(undefined, result, "pause returns nothing.");
            browser.test.sendMessage("paused");
            break;
          case "resume":
            result = await browser.geckoProfiler.resume();
            browser.test.assertEq(undefined, result, "resume returns nothing.");
            browser.test.sendMessage("resumed");
            break;
          case "test profile":
            result = await browser.geckoProfiler.getProfile();
            browser.test.assertTrue("libs" in result, "The profile contains libs.");
            browser.test.assertTrue("meta" in result, "The profile contains meta.");
            browser.test.assertTrue("threads" in result, "The profile contains threads.");
            browser.test.assertTrue(result.threads.some(t => t.name == "GeckoMain"),
                                    "The profile contains a GeckoMain thread.");
            browser.test.sendMessage("tested profile");
            break;
          case "remove runningListener":
            browser.geckoProfiler.onRunning.removeListener(runningListener);
            browser.test.sendMessage("removed runningListener");
            break;
        }
      });

      browser.test.sendMessage("ready");

      browser.geckoProfiler.onRunning.addListener(runningListener);
    },

    manifest: {
      "permissions": ["geckoProfiler"],
      "applications": {
        "gecko": {
          "id": "profilertest@mozilla.com",
        },
      },
    },
  });
};

add_task(async function testProfilerControl() {
  const acceptedExtensionIdsPref = "extensions.geckoProfiler.acceptedExtensionIds";
  Services.prefs.setCharPref(acceptedExtensionIdsPref, "profilertest@mozilla.com");

  let extension = getExtension();
  await extension.startup();
  await extension.awaitMessage("ready");
  await extension.awaitMessage("stopped");

  extension.sendMessage("start");
  await extension.awaitMessage("started");

  extension.sendMessage("test profile");
  await extension.awaitMessage("tested profile");

  extension.sendMessage("pause");
  await extension.awaitMessage("paused");

  extension.sendMessage("resume");
  await extension.awaitMessage("resumed");

  extension.sendMessage("stop");
  await extension.awaitMessage("stopped");

  extension.sendMessage("remove runningListener");
  await extension.awaitMessage("removed runningListener");

  await extension.unload();

  Services.prefs.clearUserPref(acceptedExtensionIdsPref);
});
