/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:logins",
  });
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
    SpecialPowers.clearUserPref("signon.management.page.hideMobileFooter");
  });
});

add_task(async function test_open_links() {
  // urlFinal is what the full link should be. urlBase is what gets Footer_Menu
  // appended to it.
  const linkArray = [
    {
      urlFinal: "https://example.com/android?utm_creative=Footer_Menu",
      urlBase: "https://example.com/android?utm_creative=",
      pref: "signon.management.page.mobileAndroidURL",
      selector: ".image-play-store",
    },
    {
      urlFinal: "https://example.com/apple?utm_creative=Footer_Menu",
      urlBase: "https://example.com/apple?utm_creative=",
      pref: "signon.management.page.mobileAppleURL",
      selector: ".image-app-store",
    },
  ];

  for (const { urlFinal, urlBase, pref, selector } of linkArray) {
    info("Test on " + urlFinal);

    await SpecialPowers.pushPrefEnv({
      set: [[pref, urlBase]],
    });

    let promiseNewTab = BrowserTestUtils.waitForNewTab(gBrowser, urlFinal);
    let browser = gBrowser.selectedBrowser;

    await ContentTask.spawn(browser, selector, async buttonClass => {
      let footer = Cu.waiveXrays(
        content.document
          .querySelector("login-item")
          .shadowRoot.querySelector("login-footer")
      );
      ok(!footer.hidden, "Footer is visible");
      let button = footer.shadowRoot.querySelector(buttonClass);

      button.click();
    });

    info("waiting for new tab to get opened");
    let newTab = await promiseNewTab;
    ok(true, "New tab opened to " + urlFinal);

    BrowserTestUtils.removeTab(newTab);
  }
});

add_task(async function dismissFooter() {
  let browser = gBrowser.selectedBrowser;

  await ContentTask.spawn(browser, null, async () => {
    let footer = Cu.waiveXrays(
      content.document
        .querySelector("login-item")
        .shadowRoot.querySelector("login-footer")
    );
    let dismissButton = footer.shadowRoot.querySelector(".close");

    dismissButton.click();
    await ContentTaskUtils.waitForCondition(
      () => ContentTaskUtils.is_hidden(footer),
      "Waiting for the footer to be hidden"
    );
    ok(
      ContentTaskUtils.is_hidden(footer),
      "Footer should be hidden after clicking dismiss button"
    );
  });
});
