const gHttpTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");

let gTestBrowser = null;
let gWrapperClickCount = 0;

function test() {
  waitForExplicitFinish();
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("plugins.click_to_play");
    let plugin = getTestPlugin();
    plugin.enabledState = Ci.nsIPluginTag.STATE_ENABLED;
  });
  Services.prefs.setBoolPref("plugins.click_to_play", true);
  let plugin = getTestPlugin();
  plugin.enabledState = Ci.nsIPluginTag.STATE_CLICKTOPLAY;

  gBrowser.selectedTab = gBrowser.addTab();
  gTestBrowser = gBrowser.selectedBrowser;
  gTestBrowser.addEventListener("load", pageLoad, true);
  gTestBrowser.contentWindow.location = gHttpTestRoot + "plugin_bug787619.html";
}

function pageLoad() {
  // Due to layout being async, "PluginBindAttached" may trigger later.
  // This forces a layout flush, thus triggering it, and schedules the
  // test so it is definitely executed afterwards.
  gTestBrowser.contentDocument.getElementById('plugin').clientTop;
  executeSoon(part1);
}

function part1() {
  let wrapper = gTestBrowser.contentDocument.getElementById('wrapper');
  wrapper.addEventListener('click', function() ++gWrapperClickCount, false);

  let plugin = gTestBrowser.contentDocument.getElementById('plugin');
  ok(plugin, 'got plugin element');
  ok(!plugin.activated, 'plugin should not be activated');
  ok(PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser).dismissed, "Doorhanger should not be open");

  EventUtils.synthesizeMouseAtCenter(plugin, {}, gTestBrowser.contentWindow);
  let condition = function() !PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser).dismissed;
  waitForCondition(condition, part2,
                   'waited too long for plugin to activate');
}

function part2() {
  is(gWrapperClickCount, 0, 'wrapper should not have received any clicks');
  gTestBrowser.removeEventListener("load", pageLoad, true);
  gBrowser.removeCurrentTab();
  window.focus();
  finish();
}
