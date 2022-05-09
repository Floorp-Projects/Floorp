/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  BuiltInThemes: "resource:///modules/BuiltInThemes.jsm",
});

add_task(async function colorway_closet_smoke_test() {
  await BuiltInThemes.ensureBuiltInThemes();
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "chrome://browser/content/colorwaycloset.html",
    },
    async browser => {
      const { document } = browser.contentWindow;
      ok(
        document.getElementsByTagName("colorway-selector"),
        "colorway-selector element exists"
      );
      ok(
        document.getElementByID("collection-expiry-date"),
        "expiry date element exists"
      );
    }
  );
});
