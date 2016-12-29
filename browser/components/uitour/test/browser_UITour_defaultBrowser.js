"use strict";

var gTestTab;
var gContentAPI;
var gContentWindow;
var setDefaultBrowserCalled = false;

Cc["@mozilla.org/moz/jssubscript-loader;1"]
  .getService(Ci.mozIJSSubScriptLoader)
  .loadSubScript("chrome://mochikit/content/tests/SimpleTest/MockObjects.js", this);

function MockShellService() {}
MockShellService.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIShellService]),
  isDefaultBrowser(aStartupCheck, aForAllTypes) { return false; },
  setDefaultBrowser(aClaimAllTypes, aForAllUsers) {
    setDefaultBrowserCalled = true;
  },
  shouldCheckDefaultBrowser: false,
  canSetDesktopBackground: false,
  BACKGROUND_TILE      : 1,
  BACKGROUND_STRETCH   : 2,
  BACKGROUND_CENTER    : 3,
  BACKGROUND_FILL      : 4,
  BACKGROUND_FIT       : 5,
  setDesktopBackground(aElement, aPosition) {},
  APPLICATION_MAIL : 0,
  APPLICATION_NEWS : 1,
  openApplication(aApplication) {},
  desktopBackgroundColor: 0,
  openApplicationWithURI(aApplication, aURI) {},
  defaultFeedReader: 0,
};

var mockShellService = new MockObjectRegisterer("@mozilla.org/browser/shell-service;1",
                                                MockShellService);

// Temporarily disabled, see note at test_setDefaultBrowser.
// mockShellService.register();

add_task(setup_UITourTest);

/* This test is disabled (bug 1180714) since the MockObjectRegisterer
 is not actually replacing the original ShellService.
add_UITour_task(function* test_setDefaultBrowser() {
  try {
    yield gContentAPI.setConfiguration("defaultBrowser");
    ok(setDefaultBrowserCalled, "setDefaultBrowser called");
  } finally {
    mockShellService.unregister();
  }
});
*/

add_UITour_task(function* test_isDefaultBrowser() {
  let shell = Components.classes["@mozilla.org/browser/shell-service;1"]
        .getService(Components.interfaces.nsIShellService);
  let isDefault = shell.isDefaultBrowser(false);
  let data = yield getConfigurationPromise("appinfo");
  is(isDefault, data.defaultBrowser, "gContentAPI result should match shellService.isDefaultBrowser");
});
