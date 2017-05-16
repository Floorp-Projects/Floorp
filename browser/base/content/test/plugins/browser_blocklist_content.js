var gTestBrowser = null;
var gTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");
var gChromeRoot = getRootDirectory(gTestPath);

add_task(async function() {
  registerCleanupFunction(async function() {
    clearAllPluginPermissions();
    Services.prefs.clearUserPref("extensions.blocklist.suppressUI");
    Services.prefs.clearUserPref("plugins.click_to_play");
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");
    setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Second Test Plug-in");
    await asyncSetAndUpdateBlocklist(gTestRoot + "blockNoPlugins.xml", gTestBrowser);
    resetBlocklist();
    gBrowser.removeCurrentTab();
    window.focus();
    gTestBrowser = null;
  });
});

add_task(async function() {
  Services.prefs.setBoolPref("extensions.blocklist.suppressUI", true);
  Services.prefs.setBoolPref("plugins.click_to_play", true);

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  gTestBrowser = gBrowser.selectedBrowser;

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Second Test Plug-in");

  // Prime the blocklist service, the remote service doesn't launch on startup.
  await promiseTabLoadEvent(gBrowser.selectedTab, "data:text/html,<html></html>");
  let exmsg = await promiseInitContentBlocklistSvc(gBrowser.selectedBrowser);
  ok(!exmsg, "exception: " + exmsg);
});

add_task(async function() {
  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_test.html");

  await asyncSetAndUpdateBlocklist(gTestRoot + "blockNoPlugins.xml", gTestBrowser);

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  await ContentTask.spawn(gTestBrowser, {}, async function() {
    let test = content.document.getElementById("test");
    Assert.ok(test.activated, "task 1a: test plugin should be activated!");
  });
});

// Load a fresh page, load a new plugin blocklist, then load the same page again.
add_task(async function() {
  await promiseTabLoadEvent(gBrowser.selectedTab, "data:text/html,<html>GO!</html>");
  await asyncSetAndUpdateBlocklist(gTestRoot + "blockPluginHard.xml", gTestBrowser);
  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_test.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  await ContentTask.spawn(gTestBrowser, {}, async function() {
    let test = content.document.getElementById("test");
    ok(!test.activated, "task 2a: test plugin shouldn't activate!");
  });
});

// Unload the block list and lets do this again, only this time lets
// hack around in the content blocklist service maliciously.
add_task(async function() {
  await promiseTabLoadEvent(gBrowser.selectedTab, "data:text/html,<html>GO!</html>");

  await asyncSetAndUpdateBlocklist(gTestRoot + "blockNoPlugins.xml", gTestBrowser);

  // Hack the planet! Load our blocklist shim, so we can mess with blocklist
  // return results in the content process. Active until we close our tab.
  let mm = gTestBrowser.messageManager;
  info("test 3a: loading " + gChromeRoot + "blocklist_proxy.js\n");
  mm.loadFrameScript(gChromeRoot + "blocklist_proxy.js", true);

  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_test.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  await ContentTask.spawn(gTestBrowser, {}, async function() {
    let test = content.document.getElementById("test");
    Assert.ok(test.activated, "task 3a: test plugin should be activated!");
  });
});

// Load a fresh page, load a new plugin blocklist, then load the same page again.
add_task(async function() {
  await promiseTabLoadEvent(gBrowser.selectedTab, "data:text/html,<html>GO!</html>");
  await asyncSetAndUpdateBlocklist(gTestRoot + "blockPluginHard.xml", gTestBrowser);
  await promiseTabLoadEvent(gBrowser.selectedTab, gTestRoot + "plugin_test.html");

  // Work around for delayed PluginBindingAttached
  await promiseUpdatePluginBindings(gTestBrowser);

  await ContentTask.spawn(gTestBrowser, {}, async function() {
    let test = content.document.getElementById("test");
    Assert.ok(!test.activated, "task 4a: test plugin shouldn't activate!");
  });

  await asyncSetAndUpdateBlocklist(gTestRoot + "blockNoPlugins.xml", gTestBrowser);
});
