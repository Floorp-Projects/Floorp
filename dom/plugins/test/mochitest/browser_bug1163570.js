var gTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");

// simple tab load helper, pilfered from browser plugin tests
function promiseTabLoad(tab, url, eventType="load") {
  return new Promise((resolve) => {
    function handle(event) {
      if (event.originalTarget != tab.linkedBrowser.contentDocument ||
          event.target.location.href == "about:blank" ||
          (url && event.target.location.href != url)) {
        return;
      }
      tab.linkedBrowser.removeEventListener(eventType, handle, true);
      resolve(event);
    }

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
    window.focus();
  });
});

add_task(function* () {
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED, "Test Plug-in");

  let pluginTab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  let prefTab = BrowserTestUtils.addTab(gBrowser);

  yield promiseTabLoad(pluginTab, gTestRoot + "plugin_test.html");
  yield promiseTabLoad(prefTab, "about:preferences");

  yield ContentTask.spawn(gBrowser.selectedBrowser, null, function*() {
    let doc = content.document;
    let plugin = doc.getElementById("testplugin");
    Assert.ok(!!plugin, "plugin is loaded");
  });

  let ppromise = promiseWaitForEvent(window, "MozAfterPaint");
  gBrowser.selectedTab = prefTab;
  yield ppromise;

  // We're going to switch tabs using actual mouse clicks, which helps
  // reproduce this bug.
  let tabStripContainer = document.getElementById("tabbrowser-tabs");

  // diagnosis if front end layout changes
  info("-> " + tabStripContainer.tagName); // tabs
  info("-> " + tabStripContainer.firstChild.tagName); // tab
  info("-> " + tabStripContainer.childNodes[0].label); // test harness tab
  info("-> " + tabStripContainer.childNodes[1].label); // plugin tab
  info("-> " + tabStripContainer.childNodes[2].label); // preferences tab

  for (let iteration = 0; iteration < 5; iteration++) {
    ppromise = promiseWaitForEvent(window, "MozAfterPaint");
    EventUtils.synthesizeMouseAtCenter(tabStripContainer.childNodes[1], {}, window);
    yield ppromise;

    yield ContentTask.spawn(pluginTab.linkedBrowser, null, function*() {
      let doc = content.document;
      let plugin = doc.getElementById("testplugin");
      Assert.ok(XPCNativeWrapper.unwrap(plugin).nativeWidgetIsVisible(),
        "plugin is visible");
    });

    ppromise = promiseWaitForEvent(window, "MozAfterPaint");
    EventUtils.synthesizeMouseAtCenter(tabStripContainer.childNodes[2], {}, window);
    yield ppromise;

    yield ContentTask.spawn(pluginTab.linkedBrowser, null, function*() {
      let doc = content.document;
      let plugin = doc.getElementById("testplugin");
      Assert.ok(!XPCNativeWrapper.unwrap(plugin).nativeWidgetIsVisible(),
        "plugin is hidden");
    });
  }

  gBrowser.removeTab(prefTab);
  gBrowser.removeTab(pluginTab);
});
