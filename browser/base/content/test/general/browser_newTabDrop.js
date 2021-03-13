const ANY_URL = undefined;

const { SearchTestUtils } = ChromeUtils.import(
  "resource://testing-common/SearchTestUtils.jsm"
);

SearchTestUtils.init(this);

registerCleanupFunction(async function cleanup() {
  while (gBrowser.tabs.length > 1) {
    BrowserTestUtils.removeTab(gBrowser.tabs[gBrowser.tabs.length - 1]);
  }
  await Services.search.setDefault(originalEngine);
});

let originalEngine;
add_task(async function test_setup() {
  // This test opens multiple tabs and some confirm dialogs, that takes long.
  requestLongerTimeout(2);

  // Stop search-engine loads from hitting the network
  await SearchTestUtils.installSearchExtension({
    name: "MozSearch",
    search_url: "https://example.com/",
    search_url_get_params: "q={searchTerms}",
  });
  let engine = Services.search.getEngineByName("MozSearch");
  originalEngine = await Services.search.getDefault();
  await Services.search.setDefault(engine);
});

// New Tab Button opens any link.
add_task(async function single_url() {
  await dropText("mochi.test/first", ["https://www.mochi.test/first"]);
});
add_task(async function single_url2() {
  await dropText("mochi.test/second", ["https://www.mochi.test/second"]);
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

// Open URLs ignoring non-URL.
add_task(async function multiple_urls() {
  await dropText(
    `
    mochi.test/urls0
    mochi.test/urls1
    mochi.test/urls2
    non url0
    mochi.test/urls3
    non url1
    non url2
`,
    [
      "https://www.mochi.test/urls0",
      "https://www.mochi.test/urls1",
      "https://www.mochi.test/urls2",
      "https://www.mochi.test/urls3",
    ]
  );
});

// Open single search if there's no URL.
add_task(async function multiple_text() {
  await dropText(
    `
    non url0
    non url1
    non url2
`,
    [ANY_URL]
  );
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

  // Since synthesizeDrop triggers the srcElement, need to use another button
  // that should be visible.
  let dragSrcElement = document.getElementById("back-button");
  ok(dragSrcElement, "Back button exists");
  let newTabButton = document.getElementById(
    gBrowser.tabContainer.hasAttribute("overflow")
      ? "new-tab-button"
      : "tabs-newtab-button"
  );
  ok(newTabButton, "New Tab button exists");

  let awaitDrop = BrowserTestUtils.waitForEvent(newTabButton, "drop");

  let loadedPromises = expectedURLs.map(url =>
    BrowserTestUtils.waitForNewTab(gBrowser, url, false, true)
  );

  EventUtils.synthesizeDrop(
    dragSrcElement,
    newTabButton,
    dragData,
    "link",
    window
  );

  let tabs = await Promise.all(loadedPromises);
  for (let tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }

  await awaitDrop;
  ok(true, "Got drop event");
}
