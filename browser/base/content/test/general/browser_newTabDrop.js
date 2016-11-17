registerCleanupFunction(function* cleanup() {
  while (gBrowser.tabs.length > 1) {
    yield BrowserTestUtils.removeTab(gBrowser.tabs[gBrowser.tabs.length - 1]);
  }
  Services.search.currentEngine = originalEngine;
  let engine = Services.search.getEngineByName("MozSearch");
  Services.search.removeEngine(engine);
});

let originalEngine;
add_task(function* test_setup() {
  // Stop search-engine loads from hitting the network
  Services.search.addEngineWithDetails("MozSearch", "", "", "", "GET",
                                       "http://example.com/?q={searchTerms}");
  let engine = Services.search.getEngineByName("MozSearch");
  originalEngine = Services.search.currentEngine;
  Services.search.currentEngine = engine;
});

// New Tab Button opens any link.
add_task(function*() { yield dropText("mochi.test/first", 1); });
add_task(function*() { yield dropText("javascript:'bad'", 1); });
add_task(function*() { yield dropText("jAvascript:'bad'", 1); });
add_task(function*() { yield dropText("mochi.test/second", 1); });
add_task(function*() { yield dropText("data:text/html,bad", 1); });
add_task(function*() { yield dropText("mochi.test/third", 1); });

// Single text/plain item, with multiple links.
add_task(function*() { yield dropText("mochi.test/1\nmochi.test/2", 2); });
add_task(function*() { yield dropText("javascript:'bad1'\nmochi.test/3", 2); });
add_task(function*() { yield dropText("mochi.test/4\ndata:text/html,bad1", 2); });

// Multiple text/plain items, with single and multiple links.
add_task(function*() {
  yield drop([[{type: "text/plain",
                data: "mochi.test/5"}],
              [{type: "text/plain",
                data: "mochi.test/6\nmochi.test/7"}]], 3);
});

// Single text/x-moz-url item, with multiple links.
// "text/x-moz-url" has titles in even-numbered lines.
add_task(function*() {
  yield drop([[{type: "text/x-moz-url",
                data: "mochi.test/8\nTITLE8\nmochi.test/9\nTITLE9"}]], 2);
});

// Single item with multiple types.
add_task(function*() {
  yield drop([[{type: "text/plain",
                data: "mochi.test/10"},
               {type: "text/x-moz-url",
                data: "mochi.test/11\nTITLE11"}]], 1);
});

function dropText(text, expectedTabOpenCount = 0) {
  return drop([[{type: "text/plain", data: text}]], expectedTabOpenCount);
}

function* drop(dragData, expectedTabOpenCount = 0) {
  let dragDataString = JSON.stringify(dragData);
  info(`Starting test for datagData:${dragDataString}; expectedTabOpenCount:${expectedTabOpenCount}`);
  let scriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"].
                     getService(Ci.mozIJSSubScriptLoader);
  let EventUtils = {};
  scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/EventUtils.js", EventUtils);

  // Since synthesizeDrop triggers the srcElement, need to use another button.
  let dragSrcElement = document.getElementById("downloads-button");
  ok(dragSrcElement, "Downloads button exists");
  let newTabButton = document.getElementById("new-tab-button");
  ok(newTabButton, "New Tab button exists");

  let awaitDrop = BrowserTestUtils.waitForEvent(newTabButton, "drop");
  let actualTabOpenCount = 0;
  let openedTabs = [];
  let checkCount = function(event) {
    openedTabs.push(event.target);
    actualTabOpenCount++;
    return actualTabOpenCount == expectedTabOpenCount;
  };
  let awaitTabOpen = expectedTabOpenCount && BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "TabOpen", false, checkCount);

  EventUtils.synthesizeDrop(dragSrcElement, newTabButton, dragData, "link", window);

  let tabsOpened = false;
  if (awaitTabOpen) {
    yield awaitTabOpen;
    info("Got TabOpen event");
    tabsOpened = true;
    for (let tab of openedTabs) {
      yield BrowserTestUtils.removeTab(tab);
    }
  }
  is(tabsOpened, !!expectedTabOpenCount, `Tabs for ${dragDataString} should only open if any of dropped items are valid`);

  yield awaitDrop;
  ok(true, "Got drop event");
}
