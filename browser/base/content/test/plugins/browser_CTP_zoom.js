"use strict";

var rootDir = getRootDirectory(gTestPath);
const gTestRoot = rootDir.replace(
  "chrome://mochitests/content/",
  "http://127.0.0.1:8888/"
);

var gTestBrowser = null;

add_task(async function() {
  registerCleanupFunction(function() {
    clearAllPluginPermissions();
    Services.prefs.clearUserPref("extensions.blocklist.suppressUI");
    FullZoom.reset(); // must be called before closing the tab we zoomed!
    gBrowser.removeCurrentTab();
    window.focus();
    gTestBrowser = null;
  });
});

add_task(async function() {
  Services.prefs.setBoolPref("extensions.blocklist.suppressUI", true);

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  gTestBrowser = gBrowser.selectedBrowser;

  await promiseTabLoadEvent(
    gBrowser.selectedTab,
    gTestRoot + "plugin_zoom.html"
  );

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);
});

// Enlarges the zoom level 4 times and tests that the overlay is
// visible after each enlargement.
add_task(async function() {
  for (let count = 0; count < 4; count++) {
    FullZoom.enlarge();

    // Reload the page
    await promiseTabLoadEvent(
      gBrowser.selectedTab,
      gTestRoot + "plugin_zoom.html"
    );
    await promiseUpdatePluginBindings(gTestBrowser);
    await SpecialPowers.spawn(gTestBrowser, [{ count }], async function(args) {
      let doc = content.document;
      let plugin = doc.getElementById("test");
      let overlay = plugin.openOrClosedShadowRoot.getElementById("main");
      Assert.ok(
        overlay &&
          !overlay.classList.contains("visible") &&
          overlay.getAttribute("blockall") == "blockall",
        "Overlay should be present for zoom change count " + args.count
      );
    });
  }
});
