/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.com"
);

add_task(async function test_beforeunload_stay_clears_urlbar() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.require_user_interaction_for_beforeunload", false]],
  });

  const TEST_URL = TEST_ROOT + "file_beforeunload_stop.html";
  await BrowserTestUtils.withNewTab(TEST_URL, async function (browser) {
    gURLBar.focus();
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    const inputValue = "http://example.org/?q=typed";
    gURLBar.value = inputValue.slice(0, -1);
    EventUtils.sendString(inputValue.slice(-1));

    let promptOpenedPromise = BrowserTestUtils.promiseAlertDialogOpen("cancel");
    EventUtils.synthesizeKey("VK_RETURN");
    await promptOpenedPromise;
    await TestUtils.waitForTick();

    // Can't just compare directly with TEST_URL because the URL may be trimmed.
    // Just need it to not be the example.org thing we typed in.
    ok(
      gURLBar.value.endsWith("_stop.html"),
      "Url bar should be reset to point to the stop html file"
    );
    ok(
      gURLBar.value.includes("example.com"),
      "Url bar should be reset to example.com"
    );
    // Check the lock/identity icons are back:
    is(
      gURLBar.textbox.getAttribute("pageproxystate"),
      "valid",
      "Should be in valid pageproxy state."
    );

    // Now we need to get rid of the handler to avoid the prompt coming up when trying to close the
    // tab when we exit `withNewTab`. :-)
    await SpecialPowers.spawn(browser, [], function () {
      content.window.onbeforeunload = null;
    });
  });
});
