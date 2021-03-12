/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests for draging and dropping to the Urlbar.
 */

const TEST_URL = "data:text/html,a test page";

add_task(async function test_setup() {
  // Stop search-engine loads from hitting the network.
  await SearchTestUtils.installSearchExtension();
  let originalEngine = await Services.search.getDefault();
  let engine = Services.search.getEngineByName("Example");
  await Services.search.setDefault(engine);

  registerCleanupFunction(async function cleanup() {
    while (gBrowser.tabs.length > 1) {
      BrowserTestUtils.removeTab(gBrowser.tabs[gBrowser.tabs.length - 1]);
    }
    await Services.search.setDefault(originalEngine);
  });

  if (CustomizableUI.protonToolbarEnabled) {
    CustomizableUI.addWidgetToArea("home-button", "nav-bar");
    registerCleanupFunction(() =>
      CustomizableUI.removeWidgetFromArea("home-button")
    );
  }
});

/**
 * Simulates a drop on the URL bar input field.
 * The drag source must be something different from the URL bar, so we pick the
 * home button somewhat arbitrarily.
 * @param {object} content a {type, data} object representing the DND content.
 */
function simulateURLBarDrop(content) {
  EventUtils.synthesizeDrop(
    document.getElementById("home-button"), // Dragstart element.
    gURLBar.inputField, // Drop element.
    [[content]], // Drag data.
    "copy",
    window
  );
}

add_task(async function checkDragURL() {
  await BrowserTestUtils.withNewTab(TEST_URL, function(browser) {
    info("Check dragging a normal url to the urlbar");
    const DRAG_URL = "http://www.example.com/";
    simulateURLBarDrop({ type: "text/plain", data: DRAG_URL });
    Assert.equal(
      gURLBar.value,
      TEST_URL,
      "URL bar value should not have changed"
    );
    Assert.equal(
      gBrowser.selectedBrowser.userTypedValue,
      null,
      "Stored URL bar value should not have changed"
    );
  });
});

add_task(async function checkDragForbiddenURL() {
  await BrowserTestUtils.withNewTab(TEST_URL, function(browser) {
    // See also browser_removeUnsafeProtocolsFromURLBarPaste.js for other
    // examples. In general we trust that function, we pick some testcases to
    // ensure we disallow dropping trimmed text.
    for (let url of [
      "chrome://browser/content/aboutDialog.xhtml",
      "file:///",
      "javascript:",
      "javascript:void(0)",
      "java\r\ns\ncript:void(0)",
      " javascript:void(0)",
      "\u00A0java\nscript:void(0)",
      "javascript:document.domain",
      "javascript:javascript:alert('hi!')",
    ]) {
      info(`Check dragging "{$url}" to the URL bar`);
      simulateURLBarDrop({ type: "text/plain", data: url });
      Assert.notEqual(
        gURLBar.value,
        url,
        `Shouldn't be allowed to drop ${url} on URL bar`
      );
    }
  });
});

add_task(async function checkDragText() {
  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    info("Check dragging multi word text to the urlbar");
    const TEXT = "Firefox is awesome";
    const TEXT_URL = "https://example.com/?q=Firefox+is+awesome";
    let promiseLoad = BrowserTestUtils.browserLoaded(browser, false, TEXT_URL);
    simulateURLBarDrop({ type: "text/plain", data: TEXT });
    await promiseLoad;

    info("Check dragging single word text to the urlbar");
    const WORD = "Firefox";
    const WORD_URL = "https://example.com/?q=Firefox";
    promiseLoad = BrowserTestUtils.browserLoaded(browser, false, WORD_URL);
    simulateURLBarDrop({ type: "text/plain", data: WORD });
    await promiseLoad;
  });
});
