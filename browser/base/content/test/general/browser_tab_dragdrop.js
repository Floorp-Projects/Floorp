function swapTabsAndCloseOther(a, b) {
  gBrowser.swapBrowsersAndCloseOther(gBrowser.tabs[b], gBrowser.tabs[a]);
}

var getClicks = function(tab) {
  return ContentTask.spawn(tab.linkedBrowser, {}, function() {
    return content.wrappedJSObject.clicks;
  });
}

var clickTest = Task.async(function*(tab) {
  let clicks = yield getClicks(tab);

  yield ContentTask.spawn(tab.linkedBrowser, {}, function() {
    let target = content.document.body;
    let rect = target.getBoundingClientRect();
    let left = (rect.left + rect.right) / 2;
    let top = (rect.top + rect.bottom) / 2;

    let utils = content.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                       .getInterface(Components.interfaces.nsIDOMWindowUtils);
    utils.sendMouseEvent("mousedown", left, top, 0, 1, 0, false, 0, 0);
    utils.sendMouseEvent("mouseup", left, top, 0, 1, 0, false, 0, 0);
  });

  let newClicks = yield getClicks(tab);
  is(newClicks, clicks + 1, "adding 1 more click on BODY");
});

function loadURI(tab, url) {
  tab.linkedBrowser.loadURI(url);
  return BrowserTestUtils.browserLoaded(tab.linkedBrowser);
}

// Creates a framescript which caches the current object value from the plugin
// in the page. checkObjectValue below verifies that the framescript is still
// active for the browser and that the cached value matches that from the plugin
// in the page which tells us the plugin hasn't been reinitialized.
function* cacheObjectValue(browser) {
  yield ContentTask.spawn(browser, null, function*() {
    let plugin = content.document.wrappedJSObject.body.firstChild;
    info(`plugin is ${plugin}`);
    let win = content.document.defaultView;
    info(`win is ${win}`);
    win.objectValue = plugin.getObjectValue();
    info(`got objectValue: ${win.objectValue}`);
    win.checkObjectValueListener = () => {
      let result;
      let exception;
      try {
        result = plugin.checkObjectValue(win.objectValue);
      } catch (e) {
        exception = e.toString();
      }
      info(`sending plugin.checkObjectValue(objectValue): ${result}`);
      sendAsyncMessage("Test:CheckObjectValueResult", {
        result,
        exception
      });
    };

    addMessageListener("Test:CheckObjectValue", win.checkObjectValueListener);
  });
}

// Note, can't run this via registerCleanupFunction because it needs the
// browser to still be alive and have a messageManager.
function* cleanupObjectValue(browser) {
  info("entered cleanupObjectValue")
  yield ContentTask.spawn(browser, null, function*() {
    info("in cleanup function");
    let win = content.document.defaultView;
    info(`about to delete objectValue: ${win.objectValue}`);
    delete win.objectValue;
    removeMessageListener("Test:CheckObjectValue", win.checkObjectValueListener);
    info(`about to delete checkObjectValueListener: ${win.checkObjectValueListener}`);
    delete win.checkObjectValueListener;
    info(`deleted objectValue (${win.objectValue}) and checkObjectValueListener (${win.checkObjectValueListener})`);
  });
  info("exiting cleanupObjectValue")
}

// See the notes for cacheObjectValue above.
function checkObjectValue(browser) {
  let mm = browser.messageManager;

  return new Promise((resolve, reject) => {
    let listener  = ({ data }) => {
      mm.removeMessageListener("Test:CheckObjectValueResult", listener);
      if (data.result === null) {
        ok(false, "checkObjectValue threw an exception: " + data.exception);
        reject(data.exception);
      } else {
        resolve(data.result);
      }
    };

    mm.addMessageListener("Test:CheckObjectValueResult", listener);
    mm.sendAsyncMessage("Test:CheckObjectValue");
  });
}

add_task(function*() {
  let embed = '<embed type="application/x-test" allowscriptaccess="always" allowfullscreen="true" wmode="window" width="640" height="480"></embed>'
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED);

  // create a few tabs
  let tabs = [
    gBrowser.tabs[0],
    gBrowser.addTab("about:blank", {skipAnimation: true}),
    gBrowser.addTab("about:blank", {skipAnimation: true}),
    gBrowser.addTab("about:blank", {skipAnimation: true}),
    gBrowser.addTab("about:blank", {skipAnimation: true})
  ];

  // Initially 0 1 2 3 4
  yield loadURI(tabs[1], "data:text/html;charset=utf-8,<title>tab1</title><body>tab1<iframe>");
  yield loadURI(tabs[2], "data:text/plain;charset=utf-8,tab2");
  yield loadURI(tabs[3], "data:text/html;charset=utf-8,<title>tab3</title><body>tab3<iframe>");
  yield loadURI(tabs[4], "data:text/html;charset=utf-8,<body onload='clicks=0' onclick='++clicks'>" + embed);
  yield BrowserTestUtils.switchTab(gBrowser, tabs[3]);

  swapTabsAndCloseOther(2, 3); // now: 0 1 2 4
  is(gBrowser.tabs[1], tabs[1], "tab1");
  is(gBrowser.tabs[2], tabs[3], "tab3");
  is(gBrowser.tabs[3], tabs[4], "tab4");
  delete tabs[2];

  info("about to cacheObjectValue")
  yield cacheObjectValue(tabs[4].linkedBrowser);
  info("just finished cacheObjectValue")

  swapTabsAndCloseOther(3, 2); // now: 0 1 4
  is(Array.prototype.indexOf.call(gBrowser.tabs, gBrowser.selectedTab), 2,
     "The third tab should be selected");
  delete tabs[4];


  ok((yield checkObjectValue(gBrowser.tabs[2].linkedBrowser)), "same plugin instance");

  is(gBrowser.tabs[1], tabs[1], "tab1");
  is(gBrowser.tabs[2], tabs[3], "tab4");

  let clicks = yield getClicks(gBrowser.tabs[2]);
  is(clicks, 0, "no click on BODY so far");
  yield clickTest(gBrowser.tabs[2]);

  swapTabsAndCloseOther(2, 1); // now: 0 4
  is(gBrowser.tabs[1], tabs[1], "tab1");
  delete tabs[3];

  ok((yield checkObjectValue(gBrowser.tabs[1].linkedBrowser)), "same plugin instance");
  yield cleanupObjectValue(gBrowser.tabs[1].linkedBrowser);

  yield clickTest(gBrowser.tabs[1]);

  // Load a new document (about:blank) in tab4, then detach that tab into a new window.
  // In the new window, navigate back to the original document and click on its <body>,
  // verify that its onclick was called.
  is(Array.prototype.indexOf.call(gBrowser.tabs, gBrowser.selectedTab), 1,
     "The second tab should be selected");
  is(gBrowser.tabs[1], tabs[1],
     "The second tab in gBrowser.tabs should be equal to the second tab in our array");
  is(gBrowser.selectedTab, tabs[1],
     "The second tab in our array is the selected tab");
  yield loadURI(tabs[1], "about:blank");
  let key = tabs[1].linkedBrowser.permanentKey;

  let win = gBrowser.replaceTabWithWindow(tabs[1]);
  yield new Promise(resolve => whenDelayedStartupFinished(win, resolve));
  delete tabs[1];

  // Verify that the original window now only has the initial tab left in it.
  is(gBrowser.tabs[0], tabs[0], "tab0");
  is(gBrowser.tabs[0].linkedBrowser.currentURI.spec, "about:blank", "tab0 uri");

  let tab = win.gBrowser.tabs[0];
  is(tab.linkedBrowser.permanentKey, key, "Should have kept the key");

  let awaitPageShow = BrowserTestUtils.waitForContentEvent(tab.linkedBrowser, "pageshow");
  win.gBrowser.goBack();
  yield awaitPageShow;

  yield clickTest(tab);
  promiseWindowClosed(win);
});
