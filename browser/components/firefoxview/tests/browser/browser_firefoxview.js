/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function about_firefoxview_smoke_test() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.firefox-view-next", false]],
  });

  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;

    // sanity check the important regions exist on this page
    ok(
      document.getElementById("tab-pickup-container"),
      "tab-pickup-container element exists"
    );
    ok(
      document.getElementById("recently-closed-tabs-container"),
      "recently-closed-tabs-container element exists"
    );
  });
  await SpecialPowers.popPrefEnv();
});
