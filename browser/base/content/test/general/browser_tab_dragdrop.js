// Swaps the content of tab a into tab b and then closes tab a.
function swapTabsAndCloseOther(a, b) {
  gBrowser.swapBrowsersAndCloseOther(gBrowser.tabs[b], gBrowser.tabs[a]);
}

// Mirrors the effect of the above function on an array.
function swapArrayContentsAndRemoveOther(arr, a, b) {
  arr[b] = arr[a];
  arr.splice(a, 1);
}

function checkBrowserIds(expected) {
  is(
    gBrowser.tabs.length,
    expected.length,
    "Should have the right number of tabs."
  );

  for (let [i, tab] of gBrowser.tabs.entries()) {
    is(
      tab.linkedBrowser.browserId,
      expected[i],
      `Tab ${i} should have the right browser ID.`
    );
    is(
      tab.linkedBrowser.browserId,
      tab.linkedBrowser.browsingContext.browserId,
      `Browser for tab ${i} has the same browserId as its BrowsingContext`
    );
  }
}

var getClicks = function (tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [], function () {
    return content.wrappedJSObject.clicks;
  });
};

var clickTest = async function (tab) {
  let clicks = await getClicks(tab);

  await SpecialPowers.spawn(tab.linkedBrowser, [], function () {
    let target = content.document.body;
    let rect = target.getBoundingClientRect();
    let left = (rect.left + rect.right) / 2;
    let top = (rect.top + rect.bottom) / 2;

    let utils = content.windowUtils;
    utils.sendMouseEvent("mousedown", left, top, 0, 1, 0, false, 0, 0);
    utils.sendMouseEvent("mouseup", left, top, 0, 1, 0, false, 0, 0);
  });

  let newClicks = await getClicks(tab);
  is(newClicks, clicks + 1, "adding 1 more click on BODY");
};

function loadURI(tab, url) {
  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, url);
  return BrowserTestUtils.browserLoaded(tab.linkedBrowser);
}

// Creates a framescript which caches the current object value from the plugin
// in the page. checkObjectValue below verifies that the framescript is still
// active for the browser and that the cached value matches that from the plugin
// in the page which tells us the plugin hasn't been reinitialized.
async function cacheObjectValue(browser) {
  await SpecialPowers.spawn(browser, [], () => {
    let plugin = content.document.getElementById("p").wrappedJSObject;
    info(`plugin is ${plugin}`);
    let win = content.document.defaultView;
    info(`win is ${win}`);
    win.objectValue = plugin.getObjectValue();
    info(`got objectValue: ${win.objectValue}`);
  });
}

// Note, can't run this via registerCleanupFunction because it needs the
// browser to still be alive and have a messageManager.
async function cleanupObjectValue(browser) {
  info("entered cleanupObjectValue");
  await SpecialPowers.spawn(browser, [], () => {
    info("in cleanup function");
    let win = content.document.defaultView;
    info(`about to delete objectValue: ${win.objectValue}`);
    delete win.objectValue;
  });
  info("exiting cleanupObjectValue");
}

// See the notes for cacheObjectValue above.
async function checkObjectValue(browser) {
  let data = await SpecialPowers.spawn(browser, [], () => {
    let plugin = content.document.getElementById("p").wrappedJSObject;
    let win = content.document.defaultView;
    let result, exception;
    try {
      result = plugin.checkObjectValue(win.objectValue);
    } catch (e) {
      exception = e.toString();
    }
    return {
      result,
      exception,
    };
  });

  if (data.result === null) {
    ok(false, "checkObjectValue threw an exception: " + data.exception);
    throw new Error(data.exception);
  } else {
    return data.result;
  }
}

add_task(async function () {
  // create a few tabs
  let tabs = [
    gBrowser.tabs[0],
    BrowserTestUtils.addTab(gBrowser, "about:blank", { skipAnimation: true }),
    BrowserTestUtils.addTab(gBrowser, "about:blank", { skipAnimation: true }),
    BrowserTestUtils.addTab(gBrowser, "about:blank", { skipAnimation: true }),
    BrowserTestUtils.addTab(gBrowser, "about:blank", { skipAnimation: true }),
  ];

  // Initially 0 1 2 3 4
  await loadURI(
    tabs[1],
    "data:text/html;charset=utf-8,<title>tab1</title><body>tab1<iframe>"
  );
  await loadURI(tabs[2], "data:text/plain;charset=utf-8,tab2");
  await loadURI(
    tabs[3],
    "data:text/html;charset=utf-8,<title>tab3</title><body>tab3<iframe>"
  );
  await loadURI(
    tabs[4],
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    "http://example.com/browser/browser/base/content/test/general/browser_tab_dragdrop_embed.html"
  );
  await BrowserTestUtils.switchTab(gBrowser, tabs[3]);

  let browserIds = tabs.map(t => t.linkedBrowser.browserId);
  checkBrowserIds(browserIds);

  is(gBrowser.tabs[1], tabs[1], "tab1");
  is(gBrowser.tabs[2], tabs[2], "tab2");
  is(gBrowser.tabs[3], tabs[3], "tab3");
  is(gBrowser.tabs[4], tabs[4], "tab4");

  swapTabsAndCloseOther(2, 3); // now: 0 1 2 4
  // Tab 2 is gone (what was tab 3 is displaying its content).
  tabs.splice(2, 1);
  swapArrayContentsAndRemoveOther(browserIds, 2, 3);

  is(gBrowser.tabs[1], tabs[1], "tab1");
  is(gBrowser.tabs[2], tabs[2], "tab2");
  is(gBrowser.tabs[3], tabs[3], "tab4");

  checkBrowserIds(browserIds);

  info("about to cacheObjectValue");
  await cacheObjectValue(tabs[3].linkedBrowser);
  info("just finished cacheObjectValue");

  swapTabsAndCloseOther(3, 2); // now: 0 1 4
  tabs.splice(3, 1);
  swapArrayContentsAndRemoveOther(browserIds, 3, 2);

  is(
    Array.prototype.indexOf.call(gBrowser.tabs, gBrowser.selectedTab),
    2,
    "The third tab should be selected"
  );

  checkBrowserIds(browserIds);

  ok(
    await checkObjectValue(gBrowser.tabs[2].linkedBrowser),
    "same plugin instance"
  );

  is(gBrowser.tabs[1], tabs[1], "tab1");
  is(gBrowser.tabs[2], tabs[2], "tab4");

  let clicks = await getClicks(gBrowser.tabs[2]);
  is(clicks, 0, "no click on BODY so far");
  await clickTest(gBrowser.tabs[2]);

  swapTabsAndCloseOther(2, 1); // now: 0 4
  tabs.splice(2, 1);
  swapArrayContentsAndRemoveOther(browserIds, 2, 1);

  is(gBrowser.tabs[1], tabs[1], "tab4");

  checkBrowserIds(browserIds);

  ok(
    await checkObjectValue(gBrowser.tabs[1].linkedBrowser),
    "same plugin instance"
  );
  await cleanupObjectValue(gBrowser.tabs[1].linkedBrowser);

  await clickTest(gBrowser.tabs[1]);

  // Load a new document (about:blank) in tab4, then detach that tab into a new window.
  // In the new window, navigate back to the original document and click on its <body>,
  // verify that its onclick was called.
  is(
    Array.prototype.indexOf.call(gBrowser.tabs, gBrowser.selectedTab),
    1,
    "The second tab should be selected"
  );
  is(
    gBrowser.tabs[1],
    tabs[1],
    "The second tab in gBrowser.tabs should be equal to the second tab in our array"
  );
  is(
    gBrowser.selectedTab,
    tabs[1],
    "The second tab in our array is the selected tab"
  );
  await loadURI(tabs[1], "about:blank");
  let key = tabs[1].linkedBrowser.permanentKey;

  checkBrowserIds(browserIds);

  let win = gBrowser.replaceTabWithWindow(tabs[1]);
  await new Promise(resolve => whenDelayedStartupFinished(win, resolve));

  let newWinBrowserId = browserIds[1];
  browserIds.splice(1, 1);
  checkBrowserIds(browserIds);

  // Verify that the original window now only has the initial tab left in it.
  is(gBrowser.tabs[0], tabs[0], "tab0");
  is(gBrowser.tabs[0].linkedBrowser.currentURI.spec, "about:blank", "tab0 uri");

  let tab = win.gBrowser.tabs[0];
  is(tab.linkedBrowser.permanentKey, key, "Should have kept the key");
  is(tab.linkedBrowser.browserId, newWinBrowserId, "Should have kept the ID");
  is(
    tab.linkedBrowser.browserId,
    tab.linkedBrowser.browsingContext.browserId,
    "Should have kept the ID"
  );

  let awaitPageShow = BrowserTestUtils.waitForContentEvent(
    tab.linkedBrowser,
    "pageshow"
  );
  win.gBrowser.goBack();
  await awaitPageShow;

  await clickTest(tab);
  promiseWindowClosed(win);
});
