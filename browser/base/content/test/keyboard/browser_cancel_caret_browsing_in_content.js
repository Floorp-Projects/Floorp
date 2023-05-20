/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function () {
  const kPrefName_CaretBrowsingOn = "accessibility.browsewithcaret";
  await SpecialPowers.pushPrefEnv({
    set: [
      ["accessibility.browsewithcaret_shortcut.enabled", true],
      ["accessibility.warn_on_browsewithcaret", false],
      ["test.events.async.enabled", true],
      [kPrefName_CaretBrowsingOn, false],
    ],
  });

  await BrowserTestUtils.withNewTab(
    "https://example.com/browser/browser/base/content/test/keyboard/file_empty.html",
    async function (browser) {
      await SpecialPowers.spawn(browser, [], () => {
        content.document.documentElement.scrollTop; // Flush layout.
      });
      function promiseFirstAndReplyKeyEvents(aExpectedConsume) {
        return new Promise(resolve => {
          const eventType = aExpectedConsume ? "keydown" : "keypress";
          let eventCount = 0;
          let listener = () => {
            if (++eventCount === 2) {
              window.removeEventListener(eventType, listener, {
                capture: true,
                mozSystemGroup: true,
              });
              resolve();
            }
          };
          window.addEventListener(eventType, listener, {
            capture: true,
            mozSystemGroup: true,
          });
          registerCleanupFunction(() => {
            window.removeEventListener(eventType, listener, {
              capture: true,
              mozSystemGroup: true,
            });
          });
        });
      }
      let promiseReplyF7KeyEvents = promiseFirstAndReplyKeyEvents(false);
      EventUtils.synthesizeKey("KEY_F7");
      info("Waiting reply F7 keypress event...");
      await promiseReplyF7KeyEvents;
      await TestUtils.waitForTick();
      is(
        Services.prefs.getBoolPref(kPrefName_CaretBrowsingOn),
        true,
        "F7 key should enable caret browsing mode"
      );

      await SpecialPowers.pushPrefEnv({
        set: [[kPrefName_CaretBrowsingOn, false]],
      });

      await SpecialPowers.spawn(browser, [], () => {
        content.document.documentElement.scrollTop; // Flush layout.
        content.window.addEventListener(
          "keydown",
          event => event.preventDefault(),
          { capture: true }
        );
      });
      promiseReplyF7KeyEvents = promiseFirstAndReplyKeyEvents(true);
      EventUtils.synthesizeKey("KEY_F7");
      info("Waiting for reply F7 keydown event...");
      await promiseReplyF7KeyEvents;
      try {
        info(`Checking reply keypress event is not fired...`);
        await TestUtils.waitForCondition(
          () => Services.prefs.getBoolPref(kPrefName_CaretBrowsingOn),
          "",
          100, // interval
          5 // maxTries
        );
      } catch (e) {}
      is(
        Services.prefs.getBoolPref(kPrefName_CaretBrowsingOn),
        false,
        "F7 key shouldn't enable caret browsing mode because F7 keydown event is consumed by web content"
      );
    }
  );
});
