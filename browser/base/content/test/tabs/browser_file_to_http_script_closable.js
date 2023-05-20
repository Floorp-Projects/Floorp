/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */

const TEST_FILE = fileURL("dummy_page.html");
const TEST_HTTP = httpURL("tab_that_closes.html");

// Test for Bug 1632441
add_task(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.allow_scripts_to_close_windows", false]],
  });

  await BrowserTestUtils.withNewTab(TEST_FILE, async function (fileBrowser) {
    info("Tab ready");

    // The request will open a new tab, capture the new tab and the load in it.
    info("Creating promise");
    var newTabPromise = BrowserTestUtils.waitForNewTab(
      gBrowser,
      url => {
        return url.endsWith("tab_that_closes.html");
      },
      true,
      false
    );

    // Click the link, which will post to target="_blank"
    info("Creating and clicking link");
    await SpecialPowers.spawn(fileBrowser, [TEST_HTTP], target => {
      content.open(target);
    });

    // The new tab will load.
    info("Waiting for load");
    var newTab = await newTabPromise;
    ok(newTab, "Tab is loaded");
    info("waiting for it to close");
    await BrowserTestUtils.waitForTabClosing(newTab);
    ok(true, "The test completes without a timeout");
  });

  await SpecialPowers.popPrefEnv();
});
