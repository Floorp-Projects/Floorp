function swapTabsAndCloseOther(a, b) {
  gBrowser.swapBrowsersAndCloseOther(gBrowser.tabs[b], gBrowser.tabs[a]);
}

var getClicks = function(tab) {
  return ContentTask.spawn(tab.linkedBrowser, {}, function() {
    return content.wrappedJSObject.clicks;
  });
};

var clickTest = async function(tab) {
  let clicks = await getClicks(tab);

  await ContentTask.spawn(tab.linkedBrowser, {}, function() {
    let target = content.document.body;
    let rect = target.getBoundingClientRect();
    let left = (rect.left + rect.right) / 2;
    let top = (rect.top + rect.bottom) / 2;

    let utils = content.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                       .getInterface(Components.interfaces.nsIDOMWindowUtils);
    utils.sendMouseEvent("mousedown", left, top, 0, 1, 0, false, 0, 0);
    utils.sendMouseEvent("mouseup", left, top, 0, 1, 0, false, 0, 0);
  });

  let newClicks = await getClicks(tab);
  is(newClicks, clicks + 1, "adding 1 more click on BODY");
};

function loadURI(tab, url) {
  tab.linkedBrowser.loadURI(url);
  return BrowserTestUtils.browserLoaded(tab.linkedBrowser);
}

// Creates a framescript which caches the current object value from the plugin
// in the page. checkObjectValue below verifies that the framescript is still
// active for the browser and that the cached value matches that from the plugin
// in the page which tells us the plugin hasn't been reinitialized.
async function cacheObjectValue(browser) {
  await ContentTask.spawn(browser, null, async function() {
    let plugin = content.document.getElementById("p").wrappedJSObject;
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
async function cleanupObjectValue(browser) {
  info("entered cleanupObjectValue");
  await ContentTask.spawn(browser, null, async function() {
    info("in cleanup function");
    let win = content.document.defaultView;
    info(`about to delete objectValue: ${win.objectValue}`);
    delete win.objectValue;
    removeMessageListener("Test:CheckObjectValue", win.checkObjectValueListener);
    info(`about to delete checkObjectValueListener: ${win.checkObjectValueListener}`);
    delete win.checkObjectValueListener;
    info(`deleted objectValue (${win.objectValue}) and checkObjectValueListener (${win.checkObjectValueListener})`);
  });
  info("exiting cleanupObjectValue");
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

add_task(async function() {
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_ENABLED);

  // create a few tabs
  let tabs = [
    gBrowser.tabs[0],
    BrowserTestUtils.addTab(gBrowser, "about:blank", {skipAnimation: true}),
    BrowserTestUtils.addTab(gBrowser, "about:blank", {skipAnimation: true}),
    BrowserTestUtils.addTab(gBrowser, "about:blank", {skipAnimation: true}),
    BrowserTestUtils.addTab(gBrowser, "about:blank", {skipAnimation: true})
  ];

  // Initially 0 1 2 3 4
  await loadURI(tabs[1], "data:text/html;charset=utf-8,<title>tab1</title><body>tab1<iframe>");
  await loadURI(tabs[2], "data:text/plain;charset=utf-8,tab2");
  await loadURI(tabs[3], "data:text/html;charset=utf-8,<title>tab3</title><body>tab3<iframe>");
  await loadURI(tabs[4], "http://example.com/browser/browser/base/content/test/general/browser_tab_dragdrop_embed.html");
  await BrowserTestUtils.switchTab(gBrowser, tabs[3]);

  swapTabsAndCloseOther(2, 3); // now: 0 1 2 4
  is(gBrowser.tabs[1], tabs[1], "tab1");
  is(gBrowser.tabs[2], tabs[3], "tab3");
  is(gBrowser.tabs[3], tabs[4], "tab4");
  delete tabs[2];

  info("about to cacheObjectValue");
  await cacheObjectValue(tabs[4].linkedBrowser);
  info("just finished cacheObjectValue");

  swapTabsAndCloseOther(3, 2); // now: 0 1 4
  is(Array.prototype.indexOf.call(gBrowser.tabs, gBrowser.selectedTab), 2,
     "The third tab should be selected");
  delete tabs[4];


  ok((await checkObjectValue(gBrowser.tabs[2].linkedBrowser)), "same plugin instance");

  is(gBrowser.tabs[1], tabs[1], "tab1");
  is(gBrowser.tabs[2], tabs[3], "tab4");

  let clicks = await getClicks(gBrowser.tabs[2]);
  is(clicks, 0, "no click on BODY so far");
  await clickTest(gBrowser.tabs[2]);

  swapTabsAndCloseOther(2, 1); // now: 0 4
  is(gBrowser.tabs[1], tabs[1], "tab1");
  delete tabs[3];

  ok((await checkObjectValue(gBrowser.tabs[1].linkedBrowser)), "same plugin instance");
  await cleanupObjectValue(gBrowser.tabs[1].linkedBrowser);

  await clickTest(gBrowser.tabs[1]);

  // Load a new document (about:blank) in tab4, then detach that tab into a new window.
  // In the new window, navigate back to the original document and click on its <body>,
  // verify that its onclick was called.
  is(Array.prototype.indexOf.call(gBrowser.tabs, gBrowser.selectedTab), 1,
     "The second tab should be selected");
  is(gBrowser.tabs[1], tabs[1],
     "The second tab in gBrowser.tabs should be equal to the second tab in our array");
  is(gBrowser.selectedTab, tabs[1],
     "The second tab in our array is the selected tab");
  await loadURI(tabs[1], "about:blank");
  let key = tabs[1].linkedBrowser.permanentKey;

  let win = gBrowser.replaceTabWithWindow(tabs[1]);
  await new Promise(resolve => whenDelayedStartupFinished(win, resolve));
  delete tabs[1];

  // Verify that the original window now only has the initial tab left in it.
  is(gBrowser.tabs[0], tabs[0], "tab0");
  is(gBrowser.tabs[0].linkedBrowser.currentURI.spec, "about:blank", "tab0 uri");

  let tab = win.gBrowser.tabs[0];
  is(tab.linkedBrowser.permanentKey, key, "Should have kept the key");

  let awaitPageShow = BrowserTestUtils.waitForContentEvent(tab.linkedBrowser, "pageshow");
  win.gBrowser.goBack();
  await awaitPageShow;

  await clickTest(tab);
  promiseWindowClosed(win);
});
