const { SearchTestUtils } = ChromeUtils.import(
  "resource://testing-common/SearchTestUtils.jsm"
);

SearchTestUtils.init(this);

registerCleanupFunction(async function cleanup() {
  await Services.search.setDefault(originalEngine);
});

let originalEngine;
add_task(async function test_setup() {
  // Opening multiple windows on debug build takes too long time.
  requestLongerTimeout(10);

  // Stop search-engine loads from hitting the network
  await SearchTestUtils.installSearchExtension({
    name: "MozSearch",
    search_url: "https://example.com/",
    search_url_get_params: "q={searchTerms}",
  });
  let engine = Services.search.getEngineByName("MozSearch");
  originalEngine = await Services.search.getDefault();
  await Services.search.setDefault(engine);

  // Move New Window button to nav bar, to make it possible to drag and drop.
  let { CustomizableUI } = ChromeUtils.import(
    "resource:///modules/CustomizableUI.jsm"
  );
  let origPlacement = CustomizableUI.getPlacementOfWidget("new-window-button");
  if (!origPlacement || origPlacement.area != CustomizableUI.AREA_NAVBAR) {
    CustomizableUI.addWidgetToArea(
      "new-window-button",
      CustomizableUI.AREA_NAVBAR,
      0
    );
    CustomizableUI.ensureWidgetPlacedInWindow("new-window-button", window);
    registerCleanupFunction(function() {
      CustomizableUI.removeWidgetFromArea("new-window-button");
    });
  }

  if (CustomizableUI.protonToolbarEnabled) {
    CustomizableUI.addWidgetToArea("sidebar-button", "nav-bar");
    registerCleanupFunction(() =>
      CustomizableUI.removeWidgetFromArea("sidebar-button")
    );
  }
});

// New Window Button opens any link.
add_task(async function single_url() {
  await dropText("mochi.test/first", ["https://www.mochi.test/first"]);
});
add_task(async function single_javascript() {
  await dropText("javascript:'bad'", ["about:blank"]);
});
add_task(async function single_javascript_capital() {
  await dropText("jAvascript:'bad'", ["about:blank"]);
});
add_task(async function single_url2() {
  await dropText("mochi.test/second", ["https://www.mochi.test/second"]);
});
add_task(async function single_data_url() {
  await dropText("data:text/html,bad", ["data:text/html,bad"]);
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
  await dropText("javascript:'bad1'\nmochi.test/3", [
    "about:blank",
    "https://www.mochi.test/3",
  ]);
});
add_task(async function multiple_urls_data() {
  await dropText("mochi.test/4\ndata:text/html,bad1", [
    "https://www.mochi.test/4",
    "data:text/html,bad1",
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
  await dropText(
    urls.join("\n"),
    [
      "https://www.mochi.test/accept0",
      "https://www.mochi.test/accept1",
      "https://www.mochi.test/accept2",
      "https://www.mochi.test/accept3",
      "https://www.mochi.test/accept4",
    ],
    true
  );

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
  await dropText(urls.join("\n"), [], true);

  await confirmPromise;

  await popPrefs();
});

function dropText(text, expectedURLs, ignoreFirstWindow = false) {
  return drop(
    [[{ type: "text/plain", data: text }]],
    expectedURLs,
    ignoreFirstWindow
  );
}

async function drop(dragData, expectedURLs, ignoreFirstWindow = false) {
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
  let dragSrcElement = document.getElementById("sidebar-button");
  ok(dragSrcElement, "Sidebar button exists");
  let newWindowButton = document.getElementById("new-window-button");
  ok(newWindowButton, "New Window button exists");

  let tmp = {};
  ChromeUtils.import("resource://testing-common/TestUtils.jsm", tmp);

  let awaitDrop = BrowserTestUtils.waitForEvent(newWindowButton, "drop");

  let loadedPromises = expectedURLs.map(url =>
    BrowserTestUtils.waitForNewWindow({
      url,
      anyWindow: true,
      maybeErrorPage: true,
    })
  );

  EventUtils.synthesizeDrop(
    dragSrcElement,
    newWindowButton,
    dragData,
    "link",
    window
  );

  let windows = await Promise.all(loadedPromises);
  for (let window of windows) {
    await BrowserTestUtils.closeWindow(window);
  }

  await awaitDrop;
  ok(true, "Got drop event");
}
