/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function about_firefoxview_smoke_test() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;

      // sanity check the important regions exist on this page
      ok(
        document.getElementById("tabs-pickup-container"),
        "tabs-pickup-container element exists"
      );
      ok(
        document.getElementById("colorways-active-collection-template"),
        "colorways-active-collection-template element exists"
      );
      ok(
        document.getElementById("recently-closed-tabs-container"),
        "recently-closed-tabs-container element exists"
      );
      ok(
        document.getElementById("colorways-no-collection-template"),
        "colorways-no-collection-template element exists"
      );
    }
  );
});
