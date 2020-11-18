var rootDir = getRootDirectory(gTestPath);
const gTestRoot = rootDir.replace(
  "chrome://mochitests/content/",
  "http://127.0.0.1:8888/"
);
var gTestBrowser = null;
var gPluginHost = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);

add_task(async function() {
  registerCleanupFunction(function() {
    clearAllPluginPermissions();
    Services.prefs.clearUserPref("extensions.blocklist.suppressUI");
    gBrowser.removeCurrentTab();
    window.focus();
    gTestBrowser = null;
  });
});

add_task(async function() {
  Services.prefs.setBoolPref("extensions.blocklist.suppressUI", true);

  let newTab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedTab = newTab;
  gTestBrowser = gBrowser.selectedBrowser;
});

// Test that the plugin "blockall" overlay is always present but hidden,
// regardless of whether the overlay is fully, partially, or not in the
// viewport.

// fully in viewport
add_task(async function() {
  await promiseTabLoadEvent(
    gBrowser.selectedTab,
    gTestRoot + "plugin_outsideScrollArea.html"
  );

  await SpecialPowers.spawn(gTestBrowser, [], async function() {
    let doc = content.document;
    let p = doc.createElement("embed");

    p.setAttribute("id", "test");
    p.setAttribute("type", "application/x-shockwave-flash");
    p.style.left = "0";
    p.style.bottom = "200px";

    doc.getElementById("container").appendChild(p);
  });

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  await SpecialPowers.spawn(gTestBrowser, [], async function() {
    let plugin = content.document.getElementById("test");
    let overlay = plugin.openOrClosedShadowRoot.getElementById("main");
    Assert.ok(overlay);
    Assert.ok(!overlay.getAttribute("visible"));
    Assert.ok(overlay.getAttribute("blockall") == "blockall");
  });
});

// partially in viewport
add_task(async function() {
  await promiseTabLoadEvent(
    gBrowser.selectedTab,
    gTestRoot + "plugin_outsideScrollArea.html"
  );

  await SpecialPowers.spawn(gTestBrowser, [], async function() {
    let doc = content.document;
    let p = doc.createElement("embed");

    p.setAttribute("id", "test");
    p.setAttribute("type", "application/x-shockwave-flash");
    p.style.left = "0";
    p.style.bottom = "-410px";

    doc.getElementById("container").appendChild(p);
  });

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  await SpecialPowers.spawn(gTestBrowser, [], async function() {
    let plugin = content.document.getElementById("test");
    let overlay = plugin.openOrClosedShadowRoot.getElementById("main");
    Assert.ok(overlay);
    Assert.ok(!overlay.getAttribute("visible"));
    Assert.ok(overlay.getAttribute("blockall") == "blockall");
  });
});

// not in viewport
add_task(async function() {
  await promiseTabLoadEvent(
    gBrowser.selectedTab,
    gTestRoot + "plugin_outsideScrollArea.html"
  );

  await SpecialPowers.spawn(gTestBrowser, [], async function() {
    let doc = content.document;
    let p = doc.createElement("embed");

    p.setAttribute("id", "test");
    p.setAttribute("type", "application/x-shockwave-flash");
    p.style.left = "-600px";
    p.style.bottom = "0";

    doc.getElementById("container").appendChild(p);
  });

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  await SpecialPowers.spawn(gTestBrowser, [], async function() {
    let plugin = content.document.getElementById("test");
    let overlay = plugin.openOrClosedShadowRoot.getElementById("main");
    Assert.ok(overlay);
    Assert.ok(!overlay.getAttribute("visible"));
    Assert.ok(overlay.getAttribute("blockall") == "blockall");
  });
});
