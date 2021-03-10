// TODO (Bug 1680996): Investigate why this test takes a long time.
requestLongerTimeout(2);

const ANY_URL = undefined;

registerCleanupFunction(async function cleanup() {
  while (gBrowser.tabs.length > 1) {
    BrowserTestUtils.removeTab(gBrowser.tabs[gBrowser.tabs.length - 1]);
  }
  await Services.search.setDefault(originalEngine);
  let engine = Services.search.getEngineByName("MozSearch");
  await Services.search.removeEngine(engine);
});

let originalEngine;
add_task(async function test_setup() {
  // Stop search-engine loads from hitting the network
  await Services.search.addEngineWithDetails("MozSearch", {
    method: "GET",
    template: "http://example.com/?q={searchTerms}",
  });
  let engine = Services.search.getEngineByName("MozSearch");
  originalEngine = await Services.search.getDefault();
  await Services.search.setDefault(engine);
});

add_task(async function single_url() {
  await dropText("mochi.test/first", ["https://www.mochi.test/first"]);
});
add_task(async function single_javascript() {
  await dropText("javascript:'bad'", []);
});
add_task(async function single_javascript_capital() {
  await dropText("jAvascript:'bad'", []);
});
add_task(async function single_search() {
  await dropText("search this", [ANY_URL]);
});
add_task(async function single_url2() {
  await dropText("mochi.test/second", ["https://www.mochi.test/second"]);
});
add_task(async function single_data_url() {
  await dropText("data:text/html,bad", []);
});
add_task(async function single_url3() {
  await dropText("mochi.test/third", ["https://www.mochi.test/third"]);
});

// Single text/plain item, with multiple links.
add_task(async function multiple_urls() {
  await dropText("mochi.test/1\nmochi.test/2", [
    "https://www.mochi.test/1",
    "https://www.mochi.test/2",
  ]);
});
add_task(async function multiple_urls_javascript() {
  await dropText("javascript:'bad1'\nmochi.test/3", []);
});
add_task(async function multiple_urls_data() {
  await dropText("mochi.test/4\ndata:text/html,bad1", []);
});

// Multiple text/plain items, with single and multiple links.
add_task(async function multiple_items_single_and_multiple_links() {
  await drop(
    [
      [{ type: "text/plain", data: "mochi.test/5" }],
      [{ type: "text/plain", data: "mochi.test/6\nmochi.test/7" }],
    ],
    [
      "https://www.mochi.test/5",
      "https://www.mochi.test/6",
      "https://www.mochi.test/7",
    ]
  );
});

// Single text/x-moz-url item, with multiple links.
// "text/x-moz-url" has titles in even-numbered lines.
add_task(async function single_moz_url_multiple_links() {
  await drop(
    [
      [
        {
          type: "text/x-moz-url",
          data: "mochi.test/8\nTITLE8\nmochi.test/9\nTITLE9",
        },
      ],
    ],
    ["https://www.mochi.test/8", "https://www.mochi.test/9"]
  );
});

// Single item with multiple types.
add_task(async function single_item_multiple_types() {
  await drop(
    [
      [
        { type: "text/plain", data: "mochi.test/10" },
        { type: "text/x-moz-url", data: "mochi.test/11\nTITLE11" },
      ],
    ],
    ["https://www.mochi.test/11"]
  );
});

// Warn when too many URLs are dropped.
add_task(async function multiple_tabs_under_max() {
  let urls = [];
  for (let i = 0; i < 5; i++) {
    urls.push("mochi.test/multi" + i);
  }
  await dropText(urls.join("\n"), [
    "https://www.mochi.test/multi0",
    "https://www.mochi.test/multi1",
    "https://www.mochi.test/multi2",
    "https://www.mochi.test/multi3",
    "https://www.mochi.test/multi4",
  ]);
});
add_task(async function multiple_tabs_over_max_accept() {
  await pushPrefs(["browser.tabs.maxOpenBeforeWarn", 4]);

  let confirmPromise = BrowserTestUtils.promiseAlertDialog("accept");

  let urls = [];
  for (let i = 0; i < 5; i++) {
    urls.push("mochi.test/accept" + i);
  }
  await dropText(urls.join("\n"), [
    "https://www.mochi.test/accept0",
    "https://www.mochi.test/accept1",
    "https://www.mochi.test/accept2",
    "https://www.mochi.test/accept3",
    "https://www.mochi.test/accept4",
  ]);

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
  await dropText(urls.join("\n"), []);

  await confirmPromise;

  await popPrefs();
});

function dropText(text, expectedURLs) {
  return drop([[{ type: "text/plain", data: text }]], expectedURLs);
}

async function drop(dragData, expectedURLs) {
  let dragDataString = JSON.stringify(dragData);
  info(
    `Starting test for dragData:${dragDataString}; expectedURLs.length:${expectedURLs.length}`
  );
  let EventUtils = {};
  Services.scriptloader.loadSubScript(
    "chrome://mochikit/content/tests/SimpleTest/EventUtils.js",
    EventUtils
  );

  let awaitDrop = BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "drop");

  let loadedPromises = expectedURLs.map(url =>
    BrowserTestUtils.waitForNewTab(gBrowser, url, false, true)
  );

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
  EventUtils.synthesizeDrop(
    gBrowser.selectedTab,
    gBrowser.selectedTab,
    dragData,
    "link",
    window,
    undefined,
    event
  );

  let tabs = await Promise.all(loadedPromises);
  for (let tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }

  await awaitDrop;
  ok(true, "Got drop event");
}
