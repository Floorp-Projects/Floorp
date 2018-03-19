registerCleanupFunction(async function cleanup() {
  while (gBrowser.tabs.length > 1) {
    BrowserTestUtils.removeTab(gBrowser.tabs[gBrowser.tabs.length - 1]);
  }
  Services.search.currentEngine = originalEngine;
  let engine = Services.search.getEngineByName("MozSearch");
  Services.search.removeEngine(engine);
});

let originalEngine;
add_task(async function test_setup() {
  // Stop search-engine loads from hitting the network
  Services.search.addEngineWithDetails("MozSearch", "", "", "", "GET",
                                       "http://example.com/?q={searchTerms}");
  let engine = Services.search.getEngineByName("MozSearch");
  originalEngine = Services.search.currentEngine;
  Services.search.currentEngine = engine;
});

// New Tab Button opens any link.
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
  await dropText(urls.join("\n"), 5);

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
  await dropText(urls.join("\n"), 0);

  await confirmPromise;

  await popPrefs();
});

// Open URLs ignoring non-URL.
add_task(async function multiple_urls() {
  await dropText(`
    mochi.test/urls0
    mochi.test/urls1
    mochi.test/urls2
    non url0
    mochi.test/urls3
    non url1
    non url2
`, 4);
});

// Open single search if there's no URL.
add_task(async function multiple_text() {
  await dropText(`
    non url0
    non url1
    non url2
`, 1);
});

function dropText(text, expectedTabOpenCount = 0) {
  return drop([[{type: "text/plain", data: text}]], expectedTabOpenCount);
}

async function drop(dragData, expectedTabOpenCount = 0) {
  let dragDataString = JSON.stringify(dragData);
  info(`Starting test for datagData:${dragDataString}; expectedTabOpenCount:${expectedTabOpenCount}`);
  let EventUtils = {};
  Services.scriptloader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/EventUtils.js", EventUtils);

  // Since synthesizeDrop triggers the srcElement, need to use another button.
  let dragSrcElement = document.getElementById("downloads-button");
  ok(dragSrcElement, "Downloads button exists");
  let newTabButton = document.getElementById("new-tab-button");
  ok(newTabButton, "New Tab button exists");

  let awaitDrop = BrowserTestUtils.waitForEvent(newTabButton, "drop");
  let actualTabOpenCount = 0;
  let openedTabs = [];
  let promiseTabsOpen = null;
  let resolve = null;
  let handler = function(event) {
    openedTabs.push(event.target);
    actualTabOpenCount++;
    if (actualTabOpenCount == expectedTabOpenCount && resolve) {
      resolve();
    }
  };
  gBrowser.tabContainer.addEventListener("TabOpen", handler);
  if (expectedTabOpenCount) {
    promiseTabsOpen = new Promise(r => { resolve = r; });
  }

  EventUtils.synthesizeDrop(dragSrcElement, newTabButton, dragData, "link", window);

  if (promiseTabsOpen) {
    await promiseTabsOpen;
    info("Got TabOpen event");
    for (let tab of openedTabs) {
      BrowserTestUtils.removeTab(tab);
    }
  }

  await awaitDrop;
  ok(true, "Got drop event");

  gBrowser.tabContainer.removeEventListener("TabOpen", handler);
  is(actualTabOpenCount, expectedTabOpenCount,
     "No other tabs are opened");
}
