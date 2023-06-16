/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function () {
  await BrowserTestUtils.withNewTab(
    TEST_BASE_URL + "dummy_page.html",
    async function (browser) {
      let windowOpenedPromise = BrowserTestUtils.waitForNewWindow();
      await SpecialPowers.spawn(browser, [], function () {
        content.window.open("", "_BLANK", "toolbar=no,height=300,width=500");
      });
      let newWin = await windowOpenedPromise;
      is(
        newWin.gURLBar.value,
        "about:blank",
        "Should be displaying about:blank for the opened window."
      );
      await BrowserTestUtils.closeWindow(newWin);
    }
  );
});
