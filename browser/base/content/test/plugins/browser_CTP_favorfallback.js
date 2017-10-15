var rootDir = getRootDirectory(gTestPath);
const gTestRoot = rootDir.replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
var gPluginHost = Components.classes["@mozilla.org/plugin/host;1"].getService(Components.interfaces.nsIPluginHost);

add_task(async function() {
  registerCleanupFunction(function() {
    clearAllPluginPermissions();
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Shockwave Flash");
    Services.prefs.clearUserPref("plugins.favorfallback.mode");
    Services.prefs.clearUserPref("plugins.favorfallback.rules");
  });
});

add_task(async function() {
  Services.prefs.setCharPref("plugins.favorfallback.mode", "follow-ctp");
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY, "Shockwave Flash");
});

/* The expected behavior of each testcase is documented with its markup
 * in plugin_favorfallback.html.
 *
 *  - "name" is the name of the testcase in the test file.
 *  - "rule" is how the plugins.favorfallback.rules must be configured
 *           for this testcase.
 */
const testcases = [
  {
    name: "video",
    rule: "video",
  },

  {
    name: "nosrc",
    rule: "nosrc",
  },

  {
    name: "embed",
    rule: "embed,true",
  },

  {
    name: "adobelink",
    rule: "adobelink,true",
  },

  {
    name: "installinstructions",
    rule: "installinstructions,true",
  },

];

add_task(async function() {
  for (let testcase of Object.values(testcases)) {
    info(`Running testcase ${testcase.name}`);

    Services.prefs.setCharPref("plugins.favorfallback.rules", testcase.rule);

    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      `${gTestRoot}plugin_favorfallback.html?testcase=${testcase.name}`
    );

    await ContentTask.spawn(tab.linkedBrowser, testcase.name, async function testPlugins(name) {
      let testcaseDiv = content.document.getElementById(`testcase_${name}`);
      let ctpPlugins = testcaseDiv.querySelectorAll(".expected_ctp");

      for (let ctpPlugin of ctpPlugins) {
        ok(ctpPlugin instanceof Ci.nsIObjectLoadingContent, "This is a plugin object");
        is(ctpPlugin.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY, "Plugin is CTP");
      }

      let fallbackPlugins = testcaseDiv.querySelectorAll(".expected_fallback");

      for (let fallbackPlugin of fallbackPlugins) {
        ok(fallbackPlugin instanceof Ci.nsIObjectLoadingContent, "This is a plugin object");
        is(fallbackPlugin.pluginFallbackType, Ci.nsIObjectLoadingContent.PLUGIN_ALTERNATE, "Plugin fallback content was used");
      }
    });

    await BrowserTestUtils.removeTab(tab);
  }
});
