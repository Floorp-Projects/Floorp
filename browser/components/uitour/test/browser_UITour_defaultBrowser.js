"use strict";

var gTestTab;
var gContentAPI;
var setDefaultBrowserCalled = false;

Services.scriptloader.loadSubScript(
  "chrome://mochikit/content/tests/SimpleTest/MockObjects.js",
  this
);

function MockShellService() {}
MockShellService.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsIShellService"]),
  isDefaultBrowser(aStartupCheck, aForAllTypes) {
    return false;
  },
  setDefaultBrowser(aForAllUsers) {
    setDefaultBrowserCalled = true;
  },
  shouldCheckDefaultBrowser: false,
  canSetDesktopBackground: false,
  BACKGROUND_TILE: 1,
  BACKGROUND_STRETCH: 2,
  BACKGROUND_CENTER: 3,
  BACKGROUND_FILL: 4,
  BACKGROUND_FIT: 5,
  BACKGROUND_SPAN: 6,
  setDesktopBackground(aElement, aPosition) {},
  desktopBackgroundColor: 0,
};

var mockShellService = new MockObjectRegisterer(
  "@mozilla.org/browser/shell-service;1",
  MockShellService
);

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

add_UITour_task(async function test_isDefaultBrowser() {
  let shell = Cc["@mozilla.org/browser/shell-service;1"].getService(
    Ci.nsIShellService
  );
  let isDefault = shell.isDefaultBrowser(false);
  let data = await getConfigurationPromise("appinfo");
  is(
    isDefault,
    data.defaultBrowser,
    "gContentAPI result should match shellService.isDefaultBrowser"
  );
});
