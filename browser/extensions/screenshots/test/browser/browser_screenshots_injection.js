/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

/**
 * Check that web content cannot break into screenshots.
 */
add_task(async function test_inject_srcdoc() {
  // If Screenshots was disabled, enable it just for this test.
  const addon = await AddonManager.getAddonByID("screenshots@mozilla.org");
  const isEnabled = addon.enabled;
  if (!isEnabled) {
    await addon.enable({ allowSystemAddons: true });
    registerCleanupFunction(async () => {
      await addon.disable({ allowSystemAddons: true });
    });
  }

  await BrowserTestUtils.withNewTab(
    TEST_PATH + "injection-page.html",
    async browser => {
      // Set up the content hijacking. Do this so we can see it without
      // awaiting - the promise should never resolve.
      let response = null;
      let responsePromise = SpecialPowers.spawn(browser, [], () => {
        return new Promise(resolve => {
          // We can't pass `resolve` directly because of sandboxing.
          // `responseHandler` gets invoked from the content page.
          content.wrappedJSObject.responseHandler = Cu.exportFunction(function (
            arg
          ) {
            resolve(arg);
          },
          content);
        });
      }).then(
        r => {
          ok(false, "Should not have gotten HTML but got: " + r);
          response = r;
        },
        () => {
          // Do nothing - we expect this to error when the test finishes
          // and the actor is destroyed, while the promise still hasn't
          // been resolved. We need to catch it in order not to throw
          // uncaught rejection errors and inadvertently fail the test.
        }
      );

      let error;
      let errorPromise = new Promise(resolve => {
        SpecialPowers.registerConsoleListener(msg => {
          if (
            msg.message?.match(/iframe URL does not match expected blank.html/)
          ) {
            error = msg;
            resolve();
          }
        });
      });

      // Now try to start the screenshot flow:
      CustomizableUI.addWidgetToArea(
        "screenshot-button",
        CustomizableUI.AREA_NAVBAR
      );

      let screenshotBtn = document.getElementById("screenshot-button");
      screenshotBtn.click();
      await Promise.race([errorPromise, responsePromise]);
      ok(error, "Should get the relevant error: " + error?.message);
      ok(!response, "Should not get a response from the webpage.");

      SpecialPowers.postConsoleSentinel();
    }
  );
});
