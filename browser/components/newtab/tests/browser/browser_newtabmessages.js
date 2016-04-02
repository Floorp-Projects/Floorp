/* globals Cu, XPCOMUtils, Preferences, is, registerCleanupFunction, NewTabWebChannel */

"use strict";

Cu.import("resource://gre/modules/Preferences.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NewTabWebChannel",
                                  "resource:///modules/NewTabWebChannel.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NewTabMessages",
                                  "resource:///modules/NewTabMessages.jsm");

function setup() {
  Preferences.set("browser.newtabpage.enhanced", true);
  Preferences.set("browser.newtabpage.remote.mode", "test");
  Preferences.set("browser.newtabpage.remote", true);
  NewTabMessages.init();
}

function cleanup() {
  NewTabMessages.uninit();
  Preferences.set("browser.newtabpage.remote", false);
  Preferences.set("browser.newtabpage.remote.mode", "production");
}
registerCleanupFunction(cleanup);

/*
 * Sanity tests for pref messages
 */
add_task(function* prefMessages_request() {
  setup();
  let testURL = "https://example.com/browser/browser/components/newtab/tests/browser/newtabmessages_prefs.html";

  let tabOptions = {
    gBrowser,
    url: testURL
  };

  let prefResponseAck = new Promise(resolve => {
    NewTabWebChannel.once("responseAck", () => {
      ok(true, "a request response has been received");
      resolve();
    });
  });

  yield BrowserTestUtils.withNewTab(tabOptions, function*() {
    yield prefResponseAck;
    let prefChangeAck = new Promise(resolve => {
      NewTabWebChannel.once("responseAck", () => {
        ok(true, "a change response has been received");
        resolve();
      });
    });
    Preferences.set("browser.newtabpage.enhanced", false);
    yield prefChangeAck;
  });
  cleanup();
});

/*
 * Sanity tests for preview messages
 */
add_task(function* previewMessages_request() {
  setup();
  var oldEnabledPref = Services.prefs.getBoolPref("browser.pagethumbnails.capturing_disabled");
  Services.prefs.setBoolPref("browser.pagethumbnails.capturing_disabled", false);

  let testURL = "https://example.com/browser/browser/components/newtab/tests/browser/newtabmessages_preview.html";

  let tabOptions = {
    gBrowser,
    url: testURL
  };

  let previewResponseAck = new Promise(resolve => {
    NewTabWebChannel.once("responseAck", () => {
      ok(true, "a request response has been received");
      resolve();
    });
  });

  yield BrowserTestUtils.withNewTab(tabOptions, function*() {
    yield previewResponseAck;
  });
  cleanup();
  Services.prefs.setBoolPref("browser.pagethumbnails.capturing_disabled", oldEnabledPref);
});
