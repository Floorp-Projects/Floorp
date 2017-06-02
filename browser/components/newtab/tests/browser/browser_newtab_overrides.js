"use strict";

let Cu = Components.utils;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
const PREF_NEWTAB_ACTIVITY_STREAM = "browser.newtabpage.activity-stream.enabled";

Services.prefs.setBoolPref(PREF_NEWTAB_ACTIVITY_STREAM, false);

XPCOMUtils.defineLazyServiceGetter(this, "aboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");

registerCleanupFunction(function() {
  Services.prefs.clearUserPref(PREF_NEWTAB_ACTIVITY_STREAM);
  aboutNewTabService.resetNewTabURL();
});

/*
 * Tests that the default newtab page is always returned when one types "about:newtab" in the URL bar,
 * even when overridden.
 */
add_task(async function redirector_ignores_override() {
  let overrides = [
    "chrome://browser/content/downloads/contentAreaDownloadsView.xul",
    "about:home",
  ];

  for (let overrideURL of overrides) {
    let notificationPromise = nextChangeNotificationPromise(overrideURL, `newtab page now points to ${overrideURL}`);
    aboutNewTabService.newTabURL = overrideURL;

    await notificationPromise;
    Assert.ok(aboutNewTabService.overridden, "url has been overridden");

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
    await BrowserTestUtils.withNewTab(tabOptions, async function(browser) {
      await ContentTask.spawn(browser, {}, async function() {
        Assert.equal(content.location.href, "about:newtab", "Got right URL");
        Assert.equal(content.document.location.href, "about:newtab", "Got right URL");
        Assert.equal(content.document.nodePrincipal,
          Services.scriptSecurityManager.getSystemPrincipal(),
          "nodePrincipal should match systemPrincipal");
      });
    });  // jshint ignore:line
  }
});

/*
 * Tests loading an overridden newtab page by simulating opening a newtab page from chrome
 */
add_task(async function override_loads_in_browser() {
  let overrides = [
    "chrome://browser/content/downloads/contentAreaDownloadsView.xul",
    "about:home",
    " about:home",
  ];

  for (let overrideURL of overrides) {
    let notificationPromise = nextChangeNotificationPromise(overrideURL.trim(), `newtab page now points to ${overrideURL}`);
    aboutNewTabService.newTabURL = overrideURL;

    await notificationPromise;
    Assert.ok(aboutNewTabService.overridden, "url has been overridden");

    // simulate a newtab open as a user would
    BrowserOpenTab();  // jshint ignore:line

    let browser = gBrowser.selectedBrowser;
    await BrowserTestUtils.browserLoaded(browser);

    await ContentTask.spawn(browser, {url: overrideURL}, async function(args) {
      Assert.equal(content.location.href, args.url.trim(), "Got right URL");
      Assert.equal(content.document.location.href, args.url.trim(), "Got right URL");
    });  // jshint ignore:line
    await BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
});

/*
 * Tests edge cases when someone overrides the newtabpage with whitespace
 */
add_task(async function override_blank_loads_in_browser() {
  let overrides = [
    "",
    " ",
    "\n\t",
    " about:blank",
  ];

  for (let overrideURL of overrides) {
    let notificationPromise = nextChangeNotificationPromise("about:blank", "newtab page now points to about:blank");
    aboutNewTabService.newTabURL = overrideURL;

    await notificationPromise;
    Assert.ok(aboutNewTabService.overridden, "url has been overridden");

    // simulate a newtab open as a user would
    BrowserOpenTab();  // jshint ignore:line

    let browser = gBrowser.selectedBrowser;
    await BrowserTestUtils.browserLoaded(browser);

    await ContentTask.spawn(browser, {}, async function() {
      Assert.equal(content.location.href, "about:blank", "Got right URL");
      Assert.equal(content.document.location.href, "about:blank", "Got right URL");
    });  // jshint ignore:line
    await BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
});

function nextChangeNotificationPromise(aNewURL, testMessage) {
  return TestUtils.topicObserved("newtab-url-changed", function observer(aSubject, aData) {  // jshint unused:false
      Assert.equal(aData, aNewURL, testMessage);
      return true;
  });
}
