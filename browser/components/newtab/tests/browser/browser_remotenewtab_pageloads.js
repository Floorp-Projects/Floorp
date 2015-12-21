/* globals XPCOMUtils, aboutNewTabService, Services */
"use strict";

let Cu = Components.utils;
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "RemotePageManager",
                                  "resource://gre/modules/RemotePageManager.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "aboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");

const TEST_URL = "https://example.com/browser/browser/components/newtab/tests/browser/dummy_page.html";

add_task(function* open_newtab() {
  let notificationPromise = nextChangeNotificationPromise(TEST_URL, "newtab page now points to test url");
  aboutNewTabService.newTabURL = TEST_URL;

  yield notificationPromise;
  Assert.ok(aboutNewTabService.overridden, "url has been overridden");

  let tabOptions = {
    gBrowser,
    url: "about:newtab",
  };

  yield BrowserTestUtils.withNewTab(tabOptions, function* (browser) {
    Assert.equal(TEST_URL, browser.contentWindow.location, `New tab should open to ${TEST_URL}`);
  });
});

add_task(function* emptyURL() {
  let notificationPromise = nextChangeNotificationPromise("", "newtab service now points to empty url");
  aboutNewTabService.newTabURL = "";
  yield notificationPromise;

  let tabOptions = {
    gBrowser,
    url: "about:newtab",
  };

  yield BrowserTestUtils.withNewTab(tabOptions, function* (browser) {
    Assert.equal("about:blank", browser.contentWindow.location, `New tab should open to ${"about:blank"}`);
  });
});

function nextChangeNotificationPromise(aNewURL, testMessage) {
  return new Promise(resolve => {
    Services.obs.addObserver(function observer(aSubject, aTopic, aData) {  // jshint unused:false
      Services.obs.removeObserver(observer, aTopic);
      Assert.equal(aData, aNewURL, testMessage);
      resolve();
    }, "newtab-url-changed", false);
  });
}
