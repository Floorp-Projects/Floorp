var gTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");

function waitForPluginVisibility(browser, shouldBeVisible, errorMessage) {
  return new Promise((resolve, reject) => {
    let windowUtils = window.QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIDOMWindowUtils);
    let lastTransactionId = windowUtils.lastTransactionId;
    let listener = async (event) => {
      let visibility = await ContentTask.spawn(browser, null, async function() {
        let doc = content.document;
        let plugin = doc.getElementById("testplugin");
        return XPCNativeWrapper.unwrap(plugin).nativeWidgetIsVisible();
      });

      if (visibility == shouldBeVisible) {
        window.removeEventListener("MozAfterPaint", listener);
        resolve();
      } else if (event && event.transactionId > lastTransactionId) {
        // We want to allow for one failed check since we call listener
        // directly, but if we get a MozAfterPaint notification and we
        // still don't have the correct visibility, that's likely a
        // problem.
        reject(new Error("MozAfterPaint had a mismatched plugin visibility"));
      }
    };
    window.addEventListener("MozAfterPaint", listener);
    listener(null);
  });
}

// tests that we get plugin updates when we flip between tabs that
// have the same plugin in the same position in the page.

add_task(async function() {
  let result, tabSwitchedPromise;

  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");

  let testTab = gBrowser.selectedTab;
  let pluginTab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, gTestRoot + "plugin_test.html");
  let pluginTab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, gTestRoot + "plugin_test.html");

  result = await ContentTask.spawn(pluginTab1.linkedBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("testplugin");
    return !!plugin;
  });
  is(result, true, "plugin1 is loaded");

  result = await ContentTask.spawn(pluginTab2.linkedBrowser, null, async function() {
    let doc = content.document;
    let plugin = doc.getElementById("testplugin");
    return !!plugin;
  });
  is(result, true, "plugin2 is loaded");

  // plugin tab 2 should be selected
  is(gBrowser.selectedTab == pluginTab2, true, "plugin2 is selected");

  await waitForPluginVisibility(pluginTab1.linkedBrowser,
                                false, "plugin1 should be hidden");

  await waitForPluginVisibility(pluginTab2.linkedBrowser,
                                true, "plugin2 should be visible");

  // select plugin1 tab
  tabSwitchedPromise = waitTabSwitched();
  gBrowser.selectedTab = pluginTab1;
  await tabSwitchedPromise;

  await waitForPluginVisibility(pluginTab1.linkedBrowser,
                                true, "plugin1 should be visible");

  await waitForPluginVisibility(pluginTab2.linkedBrowser,
                                false, "plugin2 should be hidden");

  // select plugin2 tab
  tabSwitchedPromise = waitTabSwitched();
  gBrowser.selectedTab = pluginTab2;
  await tabSwitchedPromise;

  await waitForPluginVisibility(pluginTab1.linkedBrowser,
                                false, "plugin1 should be hidden");

  await waitForPluginVisibility(pluginTab2.linkedBrowser,
                                true, "plugin2 should be visible");

  // select test tab
  tabSwitchedPromise = waitTabSwitched();
  gBrowser.selectedTab = testTab;
  await tabSwitchedPromise;

  await waitForPluginVisibility(pluginTab1.linkedBrowser,
                                false, "plugin1 should be hidden");

  await waitForPluginVisibility(pluginTab2.linkedBrowser,
                                false, "plugin2 should be hidden");

  gBrowser.removeTab(pluginTab1);
  gBrowser.removeTab(pluginTab2);
});
