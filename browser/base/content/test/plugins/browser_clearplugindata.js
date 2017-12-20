var gTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
var gPluginHost = Components.classes["@mozilla.org/plugin/host;1"].getService(Components.interfaces.nsIPluginHost);
var gTestBrowser = null;

// Test clearing plugin data using sanitize.js.
const testURL1 = gTestRoot + "browser_clearplugindata.html";
const testURL2 = gTestRoot + "browser_clearplugindata_noage.html";

var tempScope = {};
Services.scriptloader.loadSubScript("chrome://browser/content/sanitize.js", tempScope);
var Sanitizer = tempScope.Sanitizer;

const pluginHostIface = Ci.nsIPluginHost;
var pluginHost = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
pluginHost.QueryInterface(pluginHostIface);

var pluginTag = getTestPlugin();
var sanitizer = null;

function stored(needles) {
  let something = pluginHost.siteHasData(this.pluginTag, null);
  if (!needles)
    return something;

  if (!something)
    return false;

  for (let i = 0; i < needles.length; ++i) {
    if (!pluginHost.siteHasData(this.pluginTag, needles[i]))
      return false;
  }
  return true;
}

add_task(async function() {
  registerCleanupFunction(function() {
    clearAllPluginPermissions();
    Services.prefs.clearUserPref("plugins.click_to_play");
    Services.prefs.clearUserPref("extensions.blocklist.suppressUI");
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Second Test Plug-in");
    if (gTestBrowser) {
      gBrowser.removeCurrentTab();
    }
    window.focus();
    gTestBrowser = null;
  });

  Services.prefs.setBoolPref("plugins.click_to_play", true);

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Second Test Plug-in");
});

function setPrefs(cookies, pluginData) {
  sanitizer = new Sanitizer();
  sanitizer.ignoreTimespan = false;
  sanitizer.prefDomain = "privacy.cpd.";
  let itemPrefs = Services.prefs.getBranch(sanitizer.prefDomain);
  itemPrefs.setBoolPref("history", false);
  itemPrefs.setBoolPref("downloads", false);
  itemPrefs.setBoolPref("cache", false);
  itemPrefs.setBoolPref("cookies", cookies);
  itemPrefs.setBoolPref("formdata", false);
  itemPrefs.setBoolPref("offlineApps", false);
  itemPrefs.setBoolPref("passwords", false);
  itemPrefs.setBoolPref("sessions", false);
  itemPrefs.setBoolPref("siteSettings", false);
  itemPrefs.setBoolPref("pluginData", pluginData);
}

async function testClearingData(url) {
  // Load page to set data for the plugin.
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  gTestBrowser = gBrowser.selectedBrowser;

  await promiseTabLoadEvent(gBrowser.selectedTab, url);

  await promiseUpdatePluginBindings(gTestBrowser);

  ok(stored(["foo.com", "bar.com", "baz.com", "qux.com"]),
    "Data stored for sites");

  // Clear 20 seconds ago.
  // In the case of testURL2 the plugin will throw
  // NS_ERROR_PLUGIN_TIME_RANGE_NOT_SUPPORTED, which should result in us
  // clearing all data regardless of age.
  let now_uSec = Date.now() * 1000;
  sanitizer.range = [now_uSec - 20 * 1000000, now_uSec];
  await sanitizer.sanitize();

  if (url == testURL1) {
    ok(stored(["bar.com", "qux.com"]), "Data stored for sites");
    ok(!stored(["foo.com"]), "Data cleared for foo.com");
    ok(!stored(["baz.com"]), "Data cleared for baz.com");

    // Clear everything.
    sanitizer.range = null;
    await sanitizer.sanitize();
  }

  ok(!stored(null), "All data cleared");

  gBrowser.removeCurrentTab();
  gTestBrowser = null;
}

add_task(async function() {
  // Test when santizing cookies.
  await setPrefs(true, false);
  await testClearingData(testURL1);
  await testClearingData(testURL2);

  // Test when santizing pluginData.
  await setPrefs(false, true);
  await testClearingData(testURL1);
  await testClearingData(testURL2);
});
