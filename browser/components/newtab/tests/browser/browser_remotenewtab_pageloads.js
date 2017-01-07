/* globals Cu, XPCOMUtils, TestUtils, aboutNewTabService, ContentTask, content, is */
"use strict";

Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "aboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");

const TEST_URL = "https://example.com/browser/browser/components/newtab/tests/browser/dummy_page.html";

/*
 * Tests opening a newtab page with a remote URL. Simulates a newtab open from chrome
 */
add_task(function* open_newtab() {
  let notificationPromise = nextChangeNotificationPromise(TEST_URL, "newtab page now points to test url");
  aboutNewTabService.newTabURL = TEST_URL;

  yield notificationPromise;
  Assert.ok(aboutNewTabService.overridden, "url has been overridden");

  /*
   * Simulate a newtab open as a user would.
   *
   * Bug 1240169 - We cannot set the URL to about:newtab because that would invoke the redirector.
   * The redirector always yields the loading of a default newtab URL. We expect the user to use
   * the browser UI to access overriding URLs, for istance by click on the "+" button in the tab
   * bar, or by using the new tab shortcut key.
   */
  BrowserOpenTab();  // jshint ignore:line

  let browser = gBrowser.selectedBrowser;
  yield BrowserTestUtils.browserLoaded(browser);

  yield ContentTask.spawn(browser, {url: TEST_URL}, function*(args) {
    Assert.equal(content.document.location.href, args.url,
      "document.location should match the external resource");
    Assert.equal(content.document.documentURI, args.url,
      "document.documentURI should match the external resource");
    Assert.equal(content.document.nodePrincipal.URI.spec, args.url,
      "nodePrincipal should match the external resource");
  });
  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

function nextChangeNotificationPromise(aNewURL, testMessage) {
  return TestUtils.topicObserved("newtab-url-changed", function observer(aSubject, aData) {  // jshint unused:false
      Assert.equal(aData, aNewURL, testMessage);
      return true;
  });
}
