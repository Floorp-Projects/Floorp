/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var rootDir = getRootDirectory(gTestPath);
const gHttpTestRoot = rootDir.replace("chrome://mochitests/content/", "http://mochi.test:8888/");

// Test clearing plugin data using sanitize.js.
const testURL1 = gHttpTestRoot + "browser_clearplugindata.html";
const testURL2 = gHttpTestRoot + "browser_clearplugindata_noage.html";

let tempScope = {};
Cc["@mozilla.org/moz/jssubscript-loader;1"].getService(Ci.mozIJSSubScriptLoader)
                                           .loadSubScript("chrome://browser/content/sanitize.js", tempScope);
let Sanitizer = tempScope.Sanitizer;

const pluginHostIface = Ci.nsIPluginHost;
var pluginHost = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
pluginHost.QueryInterface(pluginHostIface);

var pluginTag = getTestPlugin();
var s;

function stored(needles) {
  var something = pluginHost.siteHasData(this.pluginTag, null);
  if (!needles)
    return something;

  if (!something)
    return false;

  for (var i = 0; i < needles.length; ++i) {
    if (!pluginHost.siteHasData(this.pluginTag, needles[i]))
      return false;
  }
  return true;
}

function test() {
  waitForExplicitFinish();
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED);

  s = new Sanitizer();
  s.ignoreTimespan = false;
  s.prefDomain = "privacy.cpd.";
  var itemPrefs = gPrefService.getBranch(s.prefDomain);
  itemPrefs.setBoolPref("history", false);
  itemPrefs.setBoolPref("downloads", false);
  itemPrefs.setBoolPref("cache", false);
  itemPrefs.setBoolPref("cookies", true); // plugin data
  itemPrefs.setBoolPref("formdata", false);
  itemPrefs.setBoolPref("offlineApps", false);
  itemPrefs.setBoolPref("passwords", false);
  itemPrefs.setBoolPref("sessions", false);
  itemPrefs.setBoolPref("siteSettings", false);

  executeSoon(test_with_age);
}

function setFinishedCallback(callback)
{
  let testPage = gBrowser.selectedBrowser.contentWindow.wrappedJSObject;
  testPage.testFinishedCallback = function() {
    setTimeout(function() {
      info("got finished callback");
      callback();
    }, 0);
  }
}

function test_with_age()
{
  // Load page to set data for the plugin.
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function () {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

    setFinishedCallback(function() {
      ok(stored(["foo.com","bar.com","baz.com","qux.com"]),
        "Data stored for sites");

      // Clear 20 seconds ago
      var now_uSec = Date.now() * 1000;
      s.range = [now_uSec - 20*1000000, now_uSec];
      s.sanitize();

      ok(stored(["bar.com","qux.com"]), "Data stored for sites");
      ok(!stored(["foo.com"]), "Data cleared for foo.com");
      ok(!stored(["baz.com"]), "Data cleared for baz.com");

      // Clear everything
      s.range = null;
      s.sanitize();

      ok(!stored(null), "All data cleared");

      gBrowser.removeCurrentTab();

      executeSoon(test_without_age);
    });
  }, true);
  content.location = testURL1;
}

function test_without_age()
{
  // Load page to set data for the plugin.
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function () {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

    setFinishedCallback(function() {
      ok(stored(["foo.com","bar.com","baz.com","qux.com"]),
        "Data stored for sites");

      // Attempt to clear 20 seconds ago. The plugin will throw
      // NS_ERROR_PLUGIN_TIME_RANGE_NOT_SUPPORTED, which should result in us
      // clearing all data regardless of age.
      var now_uSec = Date.now() * 1000;
      s.range = [now_uSec - 20*1000000, now_uSec];
      s.sanitize();

      ok(!stored(null), "All data cleared");

      gBrowser.removeCurrentTab();

      executeSoon(finish);
    });
  }, true);
  content.location = testURL2;
}

