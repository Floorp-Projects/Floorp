var gTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");

// simple tab load helper, pilfered from browser plugin tests
function promiseTabLoad(tab, url, eventType="load") {
  return new Promise((resolve, reject) => {
    function handle(event) {
      if (event.originalTarget != tab.linkedBrowser.contentDocument ||
          event.target.location.href == "about:blank" ||
          (url && event.target.location.href != url)) {
        return;
      }
      clearTimeout(timeout);
      tab.linkedBrowser.removeEventListener(eventType, handle, true);
      resolve(event);
    }

    let timeout = setTimeout(() => {
      tab.linkedBrowser.removeEventListener(eventType, handle, true);
      reject(new Error("Timed out while waiting for a '" + eventType + "'' event"));
    }, 30000);

    tab.linkedBrowser.addEventListener(eventType, handle, true, true);
    if (url) {
      tab.linkedBrowser.loadURI(url);
    }
  });
}

// dom event listener helper
function promiseWaitForEvent(object, eventName, capturing = false, chrome = false) {
  return new Promise((resolve) => {
    function listener(event) {
      object.removeEventListener(eventName, listener, capturing, chrome);
      resolve(event);
    }
    object.addEventListener(eventName, listener, capturing, chrome);
  });
}

add_task(function* () {
  registerCleanupFunction(function () {
    Services.prefs.clearUserPref("browser.uiCustomization.disableAnimation");
    window.focus();
  });
});

add_task(function* () {
  Services.prefs.setBoolPref("browser.uiCustomization.disableAnimation", true);
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");

  let pluginTab = gBrowser.selectedTab = gBrowser.addTab();
  let customizeTab = gBrowser.addTab();

  yield promiseTabLoad(pluginTab, gTestRoot + "plugin_test.html");
  yield promiseTabLoad(customizeTab, "about:customizing");

  let result = yield ContentTask.spawn(gBrowser.selectedBrowser, null, function*() {
    let doc = content.document;
    let plugin = doc.getElementById("testplugin");
    return !!plugin;
  });

  is(result, true, "plugin is loaded");

  let cpromise = promiseWaitForEvent(window.gNavToolbox, "customizationready");
  let ppromise = promiseWaitForEvent(window, "MozAfterPaint");
  gBrowser.selectedTab = customizeTab;
  yield cpromise;
  yield ppromise;

  // We're going to switch tabs using actual mouse clicks, which helps
  // reproduce this bug.
  let tabStripContainer = document.getElementById("tabbrowser-tabs");

  // diagnosis if front end layout changes
  info("-> " + tabStripContainer.tagName); // tabs
  info("-> " + tabStripContainer.firstChild.tagName); // tab
  info("-> " + tabStripContainer.childNodes[0].label); // test harness tab
  info("-> " + tabStripContainer.childNodes[1].label); // plugin tab
  info("-> " + tabStripContainer.childNodes[2].label); // customize tab

  for (let iteration = 0; iteration < 5; iteration++) {
    cpromise = promiseWaitForEvent(window.gNavToolbox, "aftercustomization");
    ppromise = promiseWaitForEvent(window, "MozAfterPaint");
    EventUtils.synthesizeMouseAtCenter(tabStripContainer.childNodes[1], {}, window);
    yield cpromise;
    yield ppromise;

    result = yield ContentTask.spawn(pluginTab.linkedBrowser, null, function*() {
      let doc = content.document;
      let plugin = doc.getElementById("testplugin");
      return XPCNativeWrapper.unwrap(plugin).nativeWidgetIsVisible();
    });

    is(result, true, "plugin is visible");

    cpromise = promiseWaitForEvent(window.gNavToolbox, "customizationready");
    ppromise = promiseWaitForEvent(window, "MozAfterPaint");
    EventUtils.synthesizeMouseAtCenter(tabStripContainer.childNodes[2], {}, window);
    yield cpromise;
    yield ppromise;

    result = yield ContentTask.spawn(pluginTab.linkedBrowser, null, function*() {
      let doc = content.document;
      let plugin = doc.getElementById("testplugin");
      return XPCNativeWrapper.unwrap(plugin).nativeWidgetIsVisible();
    });
    is(result, false, "plugin is hidden");
  }

  // wait for customize view to shutdown cleanly otherwise we get
  // a ton of error spew on shutdown.
  cpromise = promiseWaitForEvent(window.gNavToolbox, "aftercustomization");
  gBrowser.removeTab(customizeTab);
  yield cpromise;

  gBrowser.removeTab(pluginTab);
});
