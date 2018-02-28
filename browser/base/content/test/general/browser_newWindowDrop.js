registerCleanupFunction(function cleanup() {
  Services.search.currentEngine = originalEngine;
  let engine = Services.search.getEngineByName("MozSearch");
  Services.search.removeEngine(engine);
});

let originalEngine;
add_task(async function test_setup() {
  // Opening multiple windows on debug build takes too long time.
  requestLongerTimeout(10);

  // Stop search-engine loads from hitting the network
  Services.search.addEngineWithDetails("MozSearch", "", "", "", "GET",
                                       "http://example.com/?q={searchTerms}");
  let engine = Services.search.getEngineByName("MozSearch");
  originalEngine = Services.search.currentEngine;
  Services.search.currentEngine = engine;

  // Move New Window button to nav bar, to make it possible to drag and drop.
  let {CustomizableUI} = ChromeUtils.import("resource:///modules/CustomizableUI.jsm", {});
  let origPlacement = CustomizableUI.getPlacementOfWidget("new-window-button");
  if (!origPlacement || origPlacement.area != CustomizableUI.AREA_NAVBAR) {
    CustomizableUI.addWidgetToArea("new-window-button",
                                   CustomizableUI.AREA_NAVBAR,
                                   0);
    CustomizableUI.ensureWidgetPlacedInWindow("new-window-button", window);
    registerCleanupFunction(function() {
      CustomizableUI.removeWidgetFromArea("new-window-button");
    });
  }
});

// New Window Button opens any link.
add_task(async function() { await dropText("mochi.test/first", 1); });
add_task(async function() { await dropText("javascript:'bad'", 1); });
add_task(async function() { await dropText("jAvascript:'bad'", 1); });
add_task(async function() { await dropText("mochi.test/second", 1); });
add_task(async function() { await dropText("data:text/html,bad", 1); });
add_task(async function() { await dropText("mochi.test/third", 1); });

// Single text/plain item, with multiple links.
add_task(async function() { await dropText("mochi.test/1\nmochi.test/2", 2); });
add_task(async function() { await dropText("javascript:'bad1'\nmochi.test/3", 2); });
add_task(async function() { await dropText("mochi.test/4\ndata:text/html,bad1", 2); });

// Multiple text/plain items, with single and multiple links.
add_task(async function() {
  await drop([[{type: "text/plain",
                data: "mochi.test/5"}],
              [{type: "text/plain",
                data: "mochi.test/6\nmochi.test/7"}]], 3);
});

// Single text/x-moz-url item, with multiple links.
// "text/x-moz-url" has titles in even-numbered lines.
add_task(async function() {
  await drop([[{type: "text/x-moz-url",
                data: "mochi.test/8\nTITLE8\nmochi.test/9\nTITLE9"}]], 2);
});

// Single item with multiple types.
add_task(async function() {
  await drop([[{type: "text/plain",
                data: "mochi.test/10"},
               {type: "text/x-moz-url",
                data: "mochi.test/11\nTITLE11"}]], 1);
});

// Warn when too many URLs are dropped.
add_task(async function multiple_tabs_under_max() {
  let urls = [];
  for (let i = 0; i < 5; i++) {
    urls.push("mochi.test/multi" + i);
  }
  await dropText(urls.join("\n"), 5);
});
add_task(async function multiple_tabs_over_max_accept() {
  await pushPrefs(["browser.tabs.maxOpenBeforeWarn", 4]);

  let confirmPromise = BrowserTestUtils.promiseAlertDialog("accept");

  let urls = [];
  for (let i = 0; i < 5; i++) {
    urls.push("mochi.test/accept" + i);
  }
  await dropText(urls.join("\n"), 5, true);

  await confirmPromise;

  await popPrefs();
});
add_task(async function multiple_tabs_over_max_cancel() {
  await pushPrefs(["browser.tabs.maxOpenBeforeWarn", 4]);

  let confirmPromise = BrowserTestUtils.promiseAlertDialog("cancel");

  let urls = [];
  for (let i = 0; i < 5; i++) {
    urls.push("mochi.test/cancel" + i);
  }
  await dropText(urls.join("\n"), 0, true);

  await confirmPromise;

  await popPrefs();
});

function dropText(text, expectedWindowOpenCount = 0,
                  ignoreFirstWindow = false) {
  return drop([[{type: "text/plain", data: text}]], expectedWindowOpenCount,
              ignoreFirstWindow);
}

async function drop(dragData, expectedWindowOpenCount = 0,
                    ignoreFirstWindow = false) {
  let dragDataString = JSON.stringify(dragData);
  info(`Starting test for datagData:${dragDataString}; expectedWindowOpenCount:${expectedWindowOpenCount}`);
  let EventUtils = {};
  Services.scriptloader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/EventUtils.js", EventUtils);

  // Since synthesizeDrop triggers the srcElement, need to use another button.
  let dragSrcElement = document.getElementById("downloads-button");
  ok(dragSrcElement, "Downloads button exists");
  let newWindowButton = document.getElementById("new-window-button");
  ok(newWindowButton, "New Window button exists");

  let tmp = {};
  ChromeUtils.import("resource://testing-common/TestUtils.jsm", tmp);

  let awaitDrop = BrowserTestUtils.waitForEvent(newWindowButton, "drop");
  let actualWindowOpenCount = 0;
  let openedWindows = [];
  let checkCount = function(window) {
    if (ignoreFirstWindow) {
      // When dropping too many dialog, the confirm dialog is opened and
      // domwindowopened notification is dispatched for it, before opening
      // windows for dropped items.  Ignore it.
      ignoreFirstWindow = false;
      return false;
    }

    // Add observer as soon as domWindow is opened to avoid missing the topic.
    let awaitStartup = tmp.TestUtils.topicObserved("browser-delayed-startup-finished",
                                                   subject => subject == window);
    openedWindows.push([window, awaitStartup]);
    actualWindowOpenCount++;
    return actualWindowOpenCount == expectedWindowOpenCount;
  };
  let awaitWindowOpen = expectedWindowOpenCount && BrowserTestUtils.domWindowOpened(null, checkCount);

  EventUtils.synthesizeDrop(dragSrcElement, newWindowButton, dragData, "link", window);

  let windowsOpened = false;
  if (awaitWindowOpen) {
    await awaitWindowOpen;
    info("Got Window opened");
    windowsOpened = true;
    for (let [window, awaitStartup] of openedWindows.reverse()) {
      // Wait for startup before closing, to properly close the browser window.
      await awaitStartup;
      await BrowserTestUtils.closeWindow(window);
    }
  }
  is(windowsOpened, !!expectedWindowOpenCount, `Windows for ${dragDataString} should only open if any of dropped items are valid`);

  await awaitDrop;
  ok(true, "Got drop event");
}
