/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  });
});

add_task(async function test_open_feedback() {
  const menuArray = [
    [
      "https://example.com/feedback",
      "signon.management.page.feedbackURL",
      ".menuitem-feedback",
    ],
    [
      "https://example.com/faqs",
      "signon.management.page.faqURL",
      ".menuitem-faq",
    ],
    [
      "https://example.com/android",
      "signon.management.page.mobileAndroidURL",
      ".menuitem-mobile-android",
    ],
    [
      "https://example.com/apple",
      "signon.management.page.mobileAppleURL",
      ".menuitem-mobile-ios",
    ],
  ];

  for (let i = 0; i < menuArray.length; i++) {
    info("Test on " + menuArray[i][1]);

    const TEST_URL = menuArray[i][0];
    const pref = menuArray[i][1];

    await SpecialPowers.pushPrefEnv({
      set: [[pref, TEST_URL]],
    });

    let promiseNewTab = BrowserTestUtils.waitForNewTab(gBrowser, TEST_URL);

    let browser = gBrowser.selectedBrowser;
    await BrowserTestUtils.synthesizeMouseAtCenter("menu-button", {}, browser);
    await ContentTask.spawn(browser, null, async () => {
      return ContentTaskUtils.waitForCondition(() => {
        let menuButton = Cu.waiveXrays(
          content.document.querySelector("menu-button")
        );
        return !menuButton.shadowRoot.querySelector(".menu").hidden;
      }, "waiting for menu to open");
    });

    // Not using synthesizeMouseAtCenter here because the element we want clicked on
    // is in the shadow DOM. BrowserTestUtils.synthesizeMouseAtCenter/AsyncUtilsContent
    // thinks that the shadow DOM element is in another document and throws an exception
    // when trying to call element.ownerGlobal on the targeted shadow DOM node. This is
    // on file as bug 1557489. As a workaround, this manually calculates the position to click.
    let { x, y } = await ContentTask.spawn(
      browser,
      menuArray[i][2],
      async menuItemSelector => {
        let menuButton = Cu.waiveXrays(
          content.document.querySelector("menu-button")
        );
        let prefsItem = menuButton.shadowRoot.querySelector(menuItemSelector);
        return prefsItem.getBoundingClientRect();
      }
    );
    await BrowserTestUtils.synthesizeMouseAtPoint(x + 5, y + 5, {}, browser);

    info("waiting for new tab to get opened");
    let newTab = await promiseNewTab;
    ok(true, "New tab opened to https://example.com/test");

    BrowserTestUtils.removeTab(newTab);
  }
});
