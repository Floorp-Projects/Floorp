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

add_task(function*() { yield drop("mochi.test/first", true); });
add_task(function*() { yield drop("javascript:'bad'"); });
add_task(function*() { yield drop("jAvascript:'bad'"); });
add_task(function*() { yield drop("search this", true); });
add_task(function*() { yield drop("mochi.test/second", true); });
add_task(function*() { yield drop("data:text/html,bad"); });
add_task(function*() { yield drop("mochi.test/third", true); });

function* drop(text, valid = false) {
  info(`Starting test for text:${text}; valid:${valid}`);
  let scriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"].
                     getService(Ci.mozIJSSubScriptLoader);
  let EventUtils = {};
  scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/EventUtils.js", EventUtils);

  let awaitDrop = BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "drop");
  let awaitTabOpen = valid && BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "TabOpen");
  // A drop type of "link" onto an existing tab would normally trigger a
  // load in that same tab, but tabbrowser code in _getDragTargetTab treats
  // drops on the outer edges of a tab differently (loading a new tab
  // instead). Make events created by synthesizeDrop have all of their
  // coordinates set to 0 (screenX/screenY), so they're treated as drops
  // on the outer edge of the tab, thus they open new tabs.
  var event = {
    clientX: 0,
    clientY: 0,
    screenX: 0,
    screenY: 0,
  };
  EventUtils.synthesizeDrop(gBrowser.selectedTab, gBrowser.selectedTab, [[{type: "text/plain", data: text}]], "link", window, undefined, event);
  let tabOpened = false;
  if (awaitTabOpen) {
    let tabOpenEvent = yield awaitTabOpen;
    info("Got TabOpen event");
    tabOpened = true;
    yield BrowserTestUtils.removeTab(tabOpenEvent.target);
  }
  is(tabOpened, valid, `Tab for ${text} should only open if valid`);

  yield awaitDrop;
  ok(true, "Got drop event");
}
