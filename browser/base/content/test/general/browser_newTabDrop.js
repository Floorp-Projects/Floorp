/* eslint-disable @microsoft/sdl/no-insecure-url */

const ANY_URL = undefined;

const { SearchTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SearchTestUtils.sys.mjs"
);

SearchTestUtils.init(this);

registerCleanupFunction(async function cleanup() {
  while (gBrowser.tabs.length > 1) {
    BrowserTestUtils.removeTab(gBrowser.tabs[gBrowser.tabs.length - 1]);
  }
});

add_task(async function test_setup() {
  // This test opens multiple tabs and some confirm dialogs, that takes long.
  requestLongerTimeout(2);

  // Stop search-engine loads from hitting the network
  await SearchTestUtils.installSearchExtension(
    {
      name: "MozSearch",
      search_url: "https://example.com/",
      search_url_get_params: "q={searchTerms}",
    },
    { setAsDefault: true }
  );
});

// New Tab Button opens any link.
add_task(async function single_url() {
  await dropText("example.com/first", ["http://example.com/first"]);
});
add_task(async function single_url2() {
  await dropText("example.com/second", ["http://example.com/second"]);
});
add_task(async function single_url3() {
  await dropText("example.com/third", ["http://example.com/third"]);
});

// Single text/plain item, with multiple links.
add_task(async function multiple_urls() {
  await dropText("www.example.com/1\nexample.com/2", [
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    "http://www.example.com/1",
    "http://example.com/2",
  ]);
});

// Multiple text/plain items, with single and multiple links.
add_task(async function multiple_items_single_and_multiple_links() {
  await drop(
    [
      [{ type: "text/plain", data: "example.com/5" }],
      [{ type: "text/plain", data: "example.com/6\nexample.com/7" }],
    ],
    ["http://example.com/5", "http://example.com/6", "http://example.com/7"]
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
          data: "example.com/8\nTITLE8\nexample.com/9\nTITLE9",
        },
      ],
    ],
    ["http://example.com/8", "http://example.com/9"]
  );
});

// Single item with multiple types.
add_task(async function single_item_multiple_types() {
  await drop(
    [
      [
        { type: "text/plain", data: "example.com/10" },
        { type: "text/x-moz-url", data: "example.com/11\nTITLE11" },
      ],
    ],
    ["http://example.com/11"]
  );
});

// Warn when too many URLs are dropped.
add_task(async function multiple_tabs_under_max() {
  let urls = [];
  for (let i = 0; i < 5; i++) {
    urls.push("example.com/multi" + i);
  }
  await dropText(urls.join("\n"), [
    "http://example.com/multi0",
    "http://example.com/multi1",
    "http://example.com/multi2",
    "http://example.com/multi3",
    "http://example.com/multi4",
  ]);
});
add_task(async function multiple_tabs_over_max_accept() {
  await pushPrefs(["browser.tabs.maxOpenBeforeWarn", 4]);

  let confirmPromise = BrowserTestUtils.promiseAlertDialog("accept");

  let urls = [];
  for (let i = 0; i < 5; i++) {
    urls.push("example.com/accept" + i);
  }
  await dropText(urls.join("\n"), [
    "http://example.com/accept0",
    "http://example.com/accept1",
    "http://example.com/accept2",
    "http://example.com/accept3",
    "http://example.com/accept4",
  ]);

  await confirmPromise;

  await popPrefs();
});
add_task(async function multiple_tabs_over_max_cancel() {
  await pushPrefs(["browser.tabs.maxOpenBeforeWarn", 4]);

  let confirmPromise = BrowserTestUtils.promiseAlertDialog("cancel");

  let urls = [];
  for (let i = 0; i < 5; i++) {
    urls.push("example.com/cancel" + i);
  }
  await dropText(urls.join("\n"), []);

  await confirmPromise;

  await popPrefs();
});

// Open URLs ignoring non-URL.
add_task(async function multiple_urls() {
  await dropText(
    `
    example.com/urls0
    example.com/urls1
    example.com/urls2
    non url0
    example.com/urls3
    non url1
    non url2
`,
    [
      "http://example.com/urls0",
      "http://example.com/urls1",
      "http://example.com/urls2",
      "http://example.com/urls3",
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
    BrowserTestUtils.waitForNewTab(gBrowser, url, true, true)
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
