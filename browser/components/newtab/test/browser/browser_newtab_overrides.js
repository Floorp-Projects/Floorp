"use strict";

registerCleanupFunction(() => {
  AboutNewTab.resetNewTabURL();
});

function nextChangeNotificationPromise(aNewURL, testMessage) {
  return TestUtils.topicObserved(
    "newtab-url-changed",
    function observer(aSubject, aData) {
      Assert.equal(aData, aNewURL, testMessage);
      return true;
    }
  );
}

/*
 * Tests that the default newtab page is always returned when one types "about:newtab" in the URL bar,
 * even when overridden.
 */
add_task(async function redirector_ignores_override() {
  let overrides = ["chrome://browser/content/aboutRobots.xhtml", "about:home"];

  for (let overrideURL of overrides) {
    let notificationPromise = nextChangeNotificationPromise(
      overrideURL,
      `newtab page now points to ${overrideURL}`
    );
    AboutNewTab.newTabURL = overrideURL;

    await notificationPromise;
    Assert.ok(AboutNewTab.newTabURLOverridden, "url has been overridden");

    let tabOptions = {
      gBrowser,
      url: "about:newtab",
    };

    /*
     * Simulate typing "about:newtab" in the url bar.
     *
     * Bug 1240169 - We expect the redirector to lead the user to "about:newtab", the default URL,
     * due to invoking AboutRedirector. A user interacting with the chrome otherwise would lead
     * to the overriding URLs.
     */
    await BrowserTestUtils.withNewTab(tabOptions, async browser => {
      await SpecialPowers.spawn(browser, [], async () => {
        Assert.equal(content.location.href, "about:newtab", "Got right URL");
        Assert.equal(
          content.document.location.href,
          "about:newtab",
          "Got right URL"
        );
        Assert.notEqual(
          content.document.nodePrincipal,
          Services.scriptSecurityManager.getSystemPrincipal(),
          "activity stream principal should not match systemPrincipal"
        );
      });
    });
  }
});

/*
 * Tests loading an overridden newtab page by simulating opening a newtab page from chrome
 */
add_task(async function override_loads_in_browser() {
  let overrides = [
    "chrome://browser/content/aboutRobots.xhtml",
    "about:home",
    " about:home",
  ];

  for (let overrideURL of overrides) {
    let notificationPromise = nextChangeNotificationPromise(
      overrideURL.trim(),
      `newtab page now points to ${overrideURL}`
    );
    AboutNewTab.newTabURL = overrideURL;

    await notificationPromise;
    Assert.ok(AboutNewTab.newTabURLOverridden, "url has been overridden");

    // simulate a newtab open as a user would
    BrowserOpenTab();

    let browser = gBrowser.selectedBrowser;
    await BrowserTestUtils.browserLoaded(browser);

    await SpecialPowers.spawn(browser, [{ url: overrideURL }], async args => {
      Assert.equal(content.location.href, args.url.trim(), "Got right URL");
      Assert.equal(
        content.document.location.href,
        args.url.trim(),
        "Got right URL"
      );
    });
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
});

/*
 * Tests edge cases when someone overrides the newtabpage with whitespace
 */
add_task(async function override_blank_loads_in_browser() {
  let overrides = ["", " ", "\n\t", " about:blank"];

  for (let overrideURL of overrides) {
    let notificationPromise = nextChangeNotificationPromise(
      "about:blank",
      "newtab page now points to about:blank"
    );
    AboutNewTab.newTabURL = overrideURL;

    await notificationPromise;
    Assert.ok(AboutNewTab.newTabURLOverridden, "url has been overridden");

    // simulate a newtab open as a user would
    BrowserOpenTab();

    let browser = gBrowser.selectedBrowser;
    await BrowserTestUtils.browserLoaded(browser);

    await SpecialPowers.spawn(browser, [], async () => {
      Assert.equal(content.location.href, "about:blank", "Got right URL");
      Assert.equal(
        content.document.location.href,
        "about:blank",
        "Got right URL"
      );
    });
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
});
