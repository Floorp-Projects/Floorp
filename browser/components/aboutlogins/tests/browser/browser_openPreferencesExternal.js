/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_setup(async function () {
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
    {
      urlFinal:
        "https://example.com/password-manager-remember-delete-edit-logins",
      urlBase: "https://example.com/",
      pref: "app.support.baseURL",
      selector: ".menuitem-help",
    },
  ];

  for (const { urlFinal, urlBase, pref, selector } of menuArray) {
    info("Test on " + urlFinal);

    await SpecialPowers.pushPrefEnv({
      set: [[pref, urlBase]],
    });

    let promiseNewTab = BrowserTestUtils.waitForNewTab(gBrowser, urlFinal);

    let browser = gBrowser.selectedBrowser;
    await BrowserTestUtils.synthesizeMouseAtCenter("menu-button", {}, browser);
    await SpecialPowers.spawn(browser, [], async () => {
      return ContentTaskUtils.waitForCondition(() => {
        let menuButton = content.document.querySelector("menu-button");
        return !menuButton.shadowRoot.querySelector(".menu").hidden;
      }, "waiting for menu to open");
    });

    // Not using synthesizeMouseAtCenter here because the element we want clicked on
    // is in the shadow DOM and therefore requires using a function 1st argument
    // to BrowserTestUtils.synthesizeMouseAtCenter but we need to pass an
    // arbitrary selector. See bug 1557489 for more info. As a workaround, this
    // manually calculates the position to click.
    let { x, y } = await SpecialPowers.spawn(
      browser,
      [selector],
      async menuItemSelector => {
        let menuButton = content.document.querySelector("menu-button");
        let prefsItem = menuButton.shadowRoot.querySelector(menuItemSelector);
        return prefsItem.getBoundingClientRect();
      }
    );
    await BrowserTestUtils.synthesizeMouseAtPoint(x + 5, y + 5, {}, browser);

    info("waiting for new tab to get opened");
    let newTab = await promiseNewTab;
    Assert.ok(true, "New tab opened to" + urlFinal);

    BrowserTestUtils.removeTab(newTab);
  }
});
