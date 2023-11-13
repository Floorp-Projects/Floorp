/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const STARTED_AND_CANCELED_EVENTS = [
  { category: "screenshots", method: "started", object: "toolbar_button" },
  { category: "screenshots", method: "canceled", object: "toolbar_button" },
  { category: "screenshots", method: "started", object: "shortcut" },
  { category: "screenshots", method: "canceled", object: "shortcut" },
  { category: "screenshots", method: "started", object: "context_menu" },
  { category: "screenshots", method: "canceled", object: "context_menu" },
  { category: "screenshots", method: "started", object: "quick_actions" },
  { category: "screenshots", method: "canceled", object: "quick_actions" },
];

const STARTED_RETRY_EVENTS = [
  { category: "screenshots", method: "started", object: "toolbar_button" },
  { category: "screenshots", method: "selected", object: "visible" },
  { category: "screenshots", method: "started", object: "preview_retry" },
];

const CANCEL_EVENTS = [
  { category: "screenshots", method: "started", object: "toolbar_button" },
  { category: "screenshots", method: "selected", object: "full_page" },
  { category: "screenshots", method: "canceled", object: "preview_cancel" },
  { category: "screenshots", method: "started", object: "toolbar_button" },
  { category: "screenshots", method: "canceled", object: "overlay_cancel" },
];

const COPY_EVENTS = [
  { category: "screenshots", method: "started", object: "toolbar_button" },
  { category: "screenshots", method: "selected", object: "visible" },
  { category: "screenshots", method: "copy", object: "preview_copy" },
  { category: "screenshots", method: "started", object: "toolbar_button" },
  { category: "screenshots", method: "copy", object: "overlay_copy" },
];

const CONTENT_EVENTS = [
  { category: "screenshots", method: "selected", object: "region_selection" },
  { category: "screenshots", method: "selected", object: "region_selection" },
  { category: "screenshots", method: "started", object: "overlay_retry" },
  { category: "screenshots", method: "selected", object: "element" },
];

const EXTRA_OVERLAY_EVENTS = [
  { category: "screenshots", method: "started", object: "toolbar_button" },
  {
    category: "screenshots",
    method: "copy",
    object: "overlay_copy",
    extra: {
      element: "1",
      region: "1",
      move: "1",
      resize: "1",
      fullpage: "0",
      visible: "0",
    },
  },
];

const EXTRA_EVENTS = [
  { category: "screenshots", method: "started", object: "toolbar_button" },
  { category: "screenshots", method: "selected", object: "visible" },
  { category: "screenshots", method: "started", object: "preview_retry" },
  { category: "screenshots", method: "selected", object: "full_page" },
  {
    category: "screenshots",
    method: "copy",
    object: "preview_copy",
    extra: {
      element: "0",
      region: "0",
      move: "0",
      resize: "0",
      fullpage: "1",
      visible: "1",
    },
  },
];

add_task(async function test_started_and_canceled_events() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.quickactions.enabled", true],
      ["browser.urlbar.suggest.quickactions", true],
      ["browser.urlbar.shortcuts.quickactions", true],
    ],
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      await clearAllTelemetryEvents();
      let helper = new ScreenshotsHelper(browser);
      let screenshotExit;

      helper.triggerUIFromToolbar();
      await helper.waitForOverlay();

      screenshotExit = TestUtils.topicObserved("screenshots-exit");
      helper.triggerUIFromToolbar();
      await helper.waitForOverlayClosed();
      await screenshotExit;

      EventUtils.synthesizeKey("s", { shiftKey: true, accelKey: true });
      await helper.waitForOverlay();

      screenshotExit = TestUtils.topicObserved("screenshots-exit");
      EventUtils.synthesizeKey("s", { shiftKey: true, accelKey: true });
      await helper.waitForOverlayClosed();
      await screenshotExit;

      let contextMenu = document.getElementById("contentAreaContextMenu");
      let popupShownPromise = BrowserTestUtils.waitForEvent(
        contextMenu,
        "popupshown"
      );
      await BrowserTestUtils.synthesizeMouseAtPoint(
        50,
        50,
        {
          type: "contextmenu",
          button: 2,
        },
        browser
      );
      await popupShownPromise;

      contextMenu.activateItem(
        contextMenu.querySelector("#context-take-screenshot")
      );
      await helper.waitForOverlay();

      popupShownPromise = BrowserTestUtils.waitForEvent(
        contextMenu,
        "popupshown"
      );
      await BrowserTestUtils.synthesizeMouseAtPoint(
        50,
        50,
        {
          type: "contextmenu",
          button: 2,
        },
        browser
      );
      await popupShownPromise;

      screenshotExit = TestUtils.topicObserved("screenshots-exit");
      contextMenu.activateItem(
        contextMenu.querySelector("#context-take-screenshot")
      );
      await helper.waitForOverlayClosed();
      await screenshotExit;

      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value: "screenshot",
        waitForFocus: SimpleTest.waitForFocus,
      });
      let { result } = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
      Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.DYNAMIC);
      Assert.equal(result.providerName, "quickactions");

      info("Trigger the screenshot mode");
      EventUtils.synthesizeKey("KEY_ArrowDown", {}, window);
      EventUtils.synthesizeKey("KEY_Enter", {}, window);
      await helper.waitForOverlay();

      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value: "screenshot",
        waitForFocus: SimpleTest.waitForFocus,
      });
      ({ result } = await UrlbarTestUtils.getDetailsOfResultAt(window, 1));
      Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.DYNAMIC);
      Assert.equal(result.providerName, "quickactions");

      info("Trigger the screenshot mode");
      screenshotExit = TestUtils.topicObserved("screenshots-exit");
      EventUtils.synthesizeKey("KEY_ArrowDown", {}, window);
      EventUtils.synthesizeKey("KEY_Enter", {}, window);
      await helper.waitForOverlayClosed();
      await screenshotExit;

      await assertScreenshotsEvents(STARTED_AND_CANCELED_EVENTS);
    }
  );
});

add_task(async function test_started_retry() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      await clearAllTelemetryEvents();
      let helper = new ScreenshotsHelper(browser);

      helper.triggerUIFromToolbar();
      await helper.waitForOverlay();

      let panel = await helper.waitForPanel();

      let screenshotReady = TestUtils.topicObserved(
        "screenshots-preview-ready"
      );

      // click the visible page button in panel
      let visiblePageButton = panel
        .querySelector("screenshots-buttons")
        .shadowRoot.querySelector(".visible-page");
      visiblePageButton.click();
      await screenshotReady;

      let dialog = helper.getDialog();
      let retryButton = dialog._frame.contentDocument.getElementById("retry");
      ok(retryButton, "Got the retry button");
      retryButton.click();

      await helper.waitForOverlay();

      await assertScreenshotsEvents(STARTED_RETRY_EVENTS);
    }
  );
});

add_task(async function test_canceled() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      await clearAllTelemetryEvents();
      let helper = new ScreenshotsHelper(browser);

      helper.triggerUIFromToolbar();
      await helper.waitForOverlay();

      let panel = await helper.waitForPanel();

      let screenshotReady = TestUtils.topicObserved(
        "screenshots-preview-ready"
      );

      // click the full page button in panel
      let fullPageButton = panel
        .querySelector("screenshots-buttons")
        .shadowRoot.querySelector(".full-page");
      fullPageButton.click();
      await screenshotReady;

      let dialog = helper.getDialog();
      let cancelButton = dialog._frame.contentDocument.getElementById("cancel");
      ok(cancelButton, "Got the cancel button");

      let screenshotExit = TestUtils.topicObserved("screenshots-exit");
      cancelButton.click();

      await helper.waitForOverlayClosed();
      await screenshotExit;

      helper.triggerUIFromToolbar();
      await helper.waitForOverlay();

      await helper.dragOverlay(50, 50, 300, 300);
      screenshotExit = TestUtils.topicObserved("screenshots-exit");
      await helper.clickCancelButton();

      await helper.waitForOverlayClosed();
      await screenshotExit;

      await assertScreenshotsEvents(CANCEL_EVENTS);
    }
  );
});

add_task(async function test_copy() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      await clearAllTelemetryEvents();
      let helper = new ScreenshotsHelper(browser);
      let contentInfo = await helper.getContentDimensions();
      ok(contentInfo, "Got dimensions back from the content");
      let devicePixelRatio = await getContentDevicePixelRatio(browser);

      let expectedWidth = Math.floor(
        devicePixelRatio * contentInfo.clientWidth
      );
      let expectedHeight = Math.floor(
        devicePixelRatio * contentInfo.clientHeight
      );

      helper.triggerUIFromToolbar();
      info("waiting for overlay");
      await helper.waitForOverlay();

      info("waiting for panel");
      let panel = await helper.waitForPanel();

      let screenshotReady = TestUtils.topicObserved(
        "screenshots-preview-ready"
      );

      // click the visible page button in panel
      let visiblePageButton = panel
        .querySelector("screenshots-buttons")
        .shadowRoot.querySelector(".visible-page");
      visiblePageButton.click();
      info("clicked visible page, waiting for screenshots-preview-ready");
      await screenshotReady;

      let dialog = helper.getDialog();
      let copyButton = dialog._frame.contentDocument.getElementById("copy");

      let screenshotExit = TestUtils.topicObserved("screenshots-exit");
      let clipboardChanged = helper.waitForRawClipboardChange(
        expectedWidth,
        expectedHeight
      );

      // click copy button on dialog box
      info("clicking the copy button");
      copyButton.click();

      info("Waiting for clipboard change");
      await clipboardChanged;
      info("waiting for screenshot exit");
      await screenshotExit;

      helper.triggerUIFromToolbar();
      info("waiting for overlay again");
      await helper.waitForOverlay();

      await helper.dragOverlay(50, 50, 300, 300);

      clipboardChanged = helper.waitForRawClipboardChange(
        devicePixelRatio * 250,
        devicePixelRatio * 250
      );

      screenshotExit = TestUtils.topicObserved("screenshots-exit");
      await helper.clickCopyButton();

      info("Waiting for clipboard change");
      await clipboardChanged;
      info("Waiting for exit again");
      await screenshotExit;

      info("Waiting for assertScreenshotsEvents");
      await assertScreenshotsEvents(COPY_EVENTS);
    }
  );
});

add_task(async function test_content_events() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      await clearAllTelemetryEvents();
      let helper = new ScreenshotsHelper(browser);

      helper.triggerUIFromToolbar();
      await helper.waitForOverlay();

      await helper.dragOverlay(50, 50, 300, 300);

      await helper.dragOverlay(300, 300, 333, 333, "selected");

      await helper.dragOverlay(150, 150, 200, 200, "selected");

      mouse.click(11, 11);
      await helper.waitForStateChange("crosshairs");

      await helper.clickTestPageElement();

      let screenshotExit = TestUtils.topicObserved("screenshots-exit");

      await helper.clickCopyButton();

      info("Waiting for exit");
      await screenshotExit;

      await assertScreenshotsEvents(CONTENT_EVENTS, "content", false);
      await assertScreenshotsEvents(EXTRA_OVERLAY_EVENTS);
    }
  );
});

add_task(async function test_extra_telemetry() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      await clearAllTelemetryEvents();
      let helper = new ScreenshotsHelper(browser);
      let contentInfo = await helper.getContentDimensions();
      ok(contentInfo, "Got dimensions back from the content");

      helper.triggerUIFromToolbar();
      info("waiting for overlay");
      await helper.waitForOverlay();

      info("waiting for panel");
      let panel = await helper.waitForPanel();

      let screenshotReady = TestUtils.topicObserved(
        "screenshots-preview-ready"
      );

      // click the visible page button in panel
      let visiblePageButton = panel
        .querySelector("screenshots-buttons")
        .shadowRoot.querySelector(".visible-page");
      visiblePageButton.click();
      info("clicked visible page, waiting for screenshots-preview-ready");
      await screenshotReady;

      let dialog = helper.getDialog();
      let retryButton = dialog._frame.contentDocument.getElementById("retry");
      retryButton.click();

      info("waiting for panel");
      panel = await helper.waitForPanel();

      screenshotReady = TestUtils.topicObserved("screenshots-preview-ready");

      // click the full page button in panel
      let fullPageButton = panel
        .querySelector("screenshots-buttons")
        .shadowRoot.querySelector(".full-page");
      fullPageButton.click();
      await screenshotReady;

      let screenshotExit = TestUtils.topicObserved("screenshots-exit");

      dialog = helper.getDialog();
      let copyButton = dialog._frame.contentDocument.getElementById("copy");
      retryButton.click();
      // click copy button on dialog box
      info("clicking the copy button");
      copyButton.click();

      info("waiting for screenshot exit");
      await screenshotExit;

      info("Waiting for assertScreenshotsEvents");
      await assertScreenshotsEvents(EXTRA_EVENTS);
    }
  );
});
