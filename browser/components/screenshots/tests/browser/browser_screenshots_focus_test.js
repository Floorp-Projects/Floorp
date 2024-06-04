/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const SCREENSHOTS_EVENTS = [
  { category: "screenshots", method: "started", object: "toolbar_button" },
  { category: "screenshots", method: "canceled", object: "escape" },
];

const SCREENSHOTS_LAST_SCREENSHOT_METHOD_PREF =
  "screenshots.browser.component.last-screenshot-method";
const SCREENSHOTS_LAST_SAVED_METHOD_PREF =
  "screenshots.browser.component.last-saved-method";

async function restoreFocusOnEscape(initialFocusElem, helper) {
  info(
    `restoreFocusOnEscape, focusedElement: ${Services.focus.focusedElement.localName}#${Services.focus.focusedElement.id}`
  );
  is(
    window,
    BrowserWindowTracker.getTopWindow(),
    "Our window is the top window"
  );

  let gotFocus;
  if (Services.focus.focusedElement !== initialFocusElem) {
    gotFocus = BrowserTestUtils.waitForEvent(initialFocusElem, "focus");
    await SimpleTest.promiseFocus(initialFocusElem.ownerGlobal);
    Services.focus.setFocus(initialFocusElem, Services.focus.FLAG_BYKEY);
    info(
      `Waiting to place focus on initialFocusElem: ${initialFocusElem.localName}#${initialFocusElem.id}`
    );
    await gotFocus;
  }
  is(
    Services.focus.focusedElement,
    initialFocusElem,
    `The initial element #${initialFocusElem.id} has focus`
  );

  helper.assertPanelNotVisible();
  // open Screenshots with the keyboard shortcut
  info(
    "Triggering screenshots UI with the ctrl+shift+s and waiting for the panel"
  );
  EventUtils.synthesizeKey("s", { shiftKey: true, accelKey: true });

  let button = await helper.getPanelButton("#visible-page");
  info("Panel is now visible, got button: " + button.className);
  info(
    `focusedElement: ${Services.focus.focusedElement.localName}.${Services.focus.focusedElement.className}`
  );

  await BrowserTestUtils.waitForCondition(async () => {
    return button.getRootNode().activeElement === button;
  }, "The first button in the panel should have focus");

  info(
    "Sending Escape to dismiss the screenshots UI and for the panel to be closed"
  );

  let exitObserved = TestUtils.topicObserved("screenshots-exit");
  EventUtils.synthesizeKey("KEY_Escape");
  await helper.waitForPanelClosed();
  await exitObserved;
  info("Waiting for the initialFocusElem to be the focusedElement");
  await BrowserTestUtils.waitForCondition(async () => {
    return Services.focus.focusedElement === initialFocusElem;
  }, "The initially focused element should have focus");

  info(
    `Screenshots did exit, focusedElement: ${Services.focus.focusedElement.localName}#${Services.focus.focusedElement.id}`
  );
  helper.assertPanelNotVisible();
}

add_task(async function testPanelFocused() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      await clearAllTelemetryEvents();
      let helper = new ScreenshotsHelper(browser);

      info("Opening Screenshots and waiting for the panel");
      helper.triggerUIFromToolbar();

      let button = await helper.getPanelButton("#visible-page");
      info("Panel is now visible, got button: " + button.className);
      info(
        `focusedElement: ${Services.focus.focusedElement.localName}.${Services.focus.focusedElement.className}`
      );

      info("Waiting for the button to be the activeElement");
      await BrowserTestUtils.waitForCondition(() => {
        return button.getRootNode().activeElement === button;
      }, "The first button in the panel should have focus");

      info("Sending Escape to close Screenshots");
      let exitObserved = TestUtils.topicObserved("screenshots-exit");
      EventUtils.synthesizeKey("KEY_Escape");

      info("waiting for the panel to be closed");
      await helper.waitForPanelClosed();
      info("waiting for the overlay to be closed");
      await helper.waitForOverlayClosed();
      await exitObserved;

      info("Checking telemetry");
      await assertScreenshotsEvents(SCREENSHOTS_EVENTS);
      helper.assertPanelNotVisible();
    }
  );
});

add_task(async function testRestoreFocusToChromeOnEscape() {
  for (let getInitialFocusElem of [
    () => gURLBar.inputField, // A focusable HTML chrome element
    () => gBrowser.selectedTab, // The selected tab element
  ]) {
    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: TEST_PAGE,
      },
      async browser => {
        let helper = new ScreenshotsHelper(browser);
        helper.assertPanelNotVisible();
        await SimpleTest.promiseFocus(window);
        await restoreFocusOnEscape(getInitialFocusElem(), helper);
      }
    );
  }
});

add_task(async function testRestoreFocusToToolbarbuttonOnEscape() {
  const focusId = "PanelUI-menu-button"; // a toolbarbutton
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);
      helper.assertPanelNotVisible();
      let initialFocusElem = document.getElementById(focusId);
      await SimpleTest.promiseFocus(window);
      await restoreFocusOnEscape(initialFocusElem, helper);
    }
  );
}).skip(); // Bug 1867687

add_task(async function testRestoreFocusToContentOnEscape() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: 'data:text/html;charset=utf-8,%3Cinput type%3D"text" id%3D"field"%3E',
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);
      await SimpleTest.promiseFocus(browser);
      await BrowserTestUtils.synthesizeMouse("#field", 2, 2, {}, browser);

      let initialFocusElem = Services.focus.focusedElement;
      await restoreFocusOnEscape(initialFocusElem, helper);

      is(
        initialFocusElem,
        document.activeElement,
        "The browser element has focus"
      );
      let focusId = await SpecialPowers.spawn(browser, [], () => {
        return content.document.activeElement.id;
      });
      is(focusId, "field", "The button in the content document has focus");
    }
  );
});

add_task(async function test_focusLastUsedMethod() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [SCREENSHOTS_LAST_SCREENSHOT_METHOD_PREF, ""],
      [SCREENSHOTS_LAST_SAVED_METHOD_PREF, ""],
      ["browser.download.useDownloadDir", true],
    ],
  });

  let publicDownloads = await Downloads.getList(Downloads.PUBLIC);
  // First ensure we catch the download finishing.
  let downloadFinishedPromise = new Promise(resolve => {
    publicDownloads.addView({
      onDownloadChanged(download) {
        info("Download changed!");
        if (download.succeeded || download.error) {
          info("Download succeeded or errored");
          publicDownloads.removeView(this);
          resolve(download);
        }
      },
    });
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: SHORT_TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);
      helper.triggerUIFromToolbar();
      await helper.waitForOverlay();

      let expectedFocusedButton = await helper.getPanelButton("#visible-page");

      await BrowserTestUtils.waitForCondition(() => {
        return (
          expectedFocusedButton.getRootNode().activeElement ===
          expectedFocusedButton
        );
      }, "The visible button in the panel should have focus");

      is(
        Services.focus.focusedElement,
        expectedFocusedButton,
        "The visible button in the panel should have focus"
      );

      let screenshotReady = TestUtils.topicObserved(
        "screenshots-preview-ready"
      );
      let fullpageButton = await helper.getPanelButton("#full-page");
      fullpageButton.click();
      await screenshotReady;

      let retryButton = helper.getDialogButton("retry");
      retryButton.click();

      await helper.waitForOverlay();

      expectedFocusedButton = await helper.getPanelButton("#full-page");

      await BrowserTestUtils.waitForCondition(() => {
        return (
          expectedFocusedButton.getRootNode().activeElement ===
          expectedFocusedButton
        );
      }, "The full page button in the panel should have focus");

      is(
        Services.focus.focusedElement,
        expectedFocusedButton,
        "The full button in the panel should have focus"
      );

      screenshotReady = TestUtils.topicObserved("screenshots-preview-ready");
      let visiblepageButton = await helper.getPanelButton("#visible-page");
      visiblepageButton.click();
      await screenshotReady;

      retryButton = helper.getDialogButton("retry");
      retryButton.click();

      await helper.waitForOverlay();

      expectedFocusedButton = await helper.getPanelButton("#visible-page");

      await BrowserTestUtils.waitForCondition(() => {
        return (
          expectedFocusedButton.getRootNode().activeElement ===
          expectedFocusedButton
        );
      }, "The visible button in the panel should have focus");

      is(
        Services.focus.focusedElement,
        expectedFocusedButton,
        "The visible button in the panel should have focus"
      );

      screenshotReady = TestUtils.topicObserved("screenshots-preview-ready");
      expectedFocusedButton.click();
      await screenshotReady;

      expectedFocusedButton = helper.getDialogButton("download");

      await BrowserTestUtils.waitForCondition(() => {
        return (
          expectedFocusedButton.getRootNode().activeElement ===
          expectedFocusedButton
        );
      }, "The download button in the preview dialog should have focus");

      is(
        Services.focus.focusedElement,
        expectedFocusedButton.buttonEl,
        "The download button in the preview dialog should have focus"
      );

      let screenshotExit = TestUtils.topicObserved("screenshots-exit");
      let copyButton = helper.getDialogButton("copy");
      copyButton.click();
      await screenshotExit;

      helper.triggerUIFromToolbar();
      await helper.waitForOverlay();

      let visibleButton = await helper.getPanelButton("#visible-page");

      screenshotReady = TestUtils.topicObserved("screenshots-preview-ready");
      visibleButton.click();
      await screenshotReady;

      expectedFocusedButton = helper.getDialogButton("copy");

      await BrowserTestUtils.waitForCondition(() => {
        return (
          expectedFocusedButton.getRootNode().activeElement ===
          expectedFocusedButton
        );
      }, "The copy button in the preview dialog should have focus");

      is(
        Services.focus.focusedElement,
        expectedFocusedButton.buttonEl,
        "The copy button in the preview dialog should have focus"
      );

      screenshotExit = TestUtils.topicObserved("screenshots-exit");
      let downloadButton = helper.getDialogButton("download");
      downloadButton.click();

      await Promise.all([screenshotExit, downloadFinishedPromise]);

      await publicDownloads.removeFinished();

      helper.triggerUIFromToolbar();
      await helper.waitForOverlay();

      visibleButton = await helper.getPanelButton("#visible-page");

      screenshotReady = TestUtils.topicObserved("screenshots-preview-ready");
      visibleButton.click();
      await screenshotReady;

      expectedFocusedButton = helper.getDialogButton("download");

      await BrowserTestUtils.waitForCondition(() => {
        return (
          expectedFocusedButton.getRootNode().activeElement ===
          expectedFocusedButton
        );
      }, "The download button in the preview dialog should have focus");

      is(
        Services.focus.focusedElement,
        expectedFocusedButton.buttonEl,
        "The download button in the preview dialog should have focus"
      );

      screenshotExit = TestUtils.topicObserved("screenshots-exit");
      helper.triggerUIFromToolbar();
      await screenshotExit;
    }
  );

  await SpecialPowers.popPrefEnv();
});

add_task(async function testFocusedIsLocked() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      let helper = new ScreenshotsHelper(browser);
      helper.triggerUIFromToolbar();
      await helper.waitForOverlay();

      let panel = await helper.waitForPanel();
      let mozButtonGroup = panel
        .querySelector("screenshots-buttons")
        .shadowRoot.querySelector("moz-button-group");
      let firstButton = mozButtonGroup.firstElementChild;
      let lastButton = mozButtonGroup.lastElementChild;

      firstButton.focus();

      await BrowserTestUtils.waitForCondition(() => {
        return firstButton.getRootNode().activeElement === firstButton;
      }, "The first button in the panel should have focus");
      info(
        `Actual focused id: ${Services.focus.focusedElement.id}. Expected focused id: ${firstButton.id}`
      );
      is(
        Services.focus.focusedElement,
        firstButton,
        "The first button in the panel should have focus"
      );

      EventUtils.synthesizeKey("KEY_Tab");

      await BrowserTestUtils.waitForCondition(() => {
        return lastButton.getRootNode().activeElement === lastButton;
      }, "The last button in the panel should have focus");
      info(
        `Actual focused id: ${Services.focus.focusedElement.id}. Expected focused id: ${lastButton.id}`
      );
      is(
        Services.focus.focusedElement,
        lastButton,
        "The last button in the panel should have focus"
      );

      EventUtils.synthesizeKey("KEY_Tab");

      // Focus should move to the content document
      await BrowserTestUtils.waitForCondition(() => {
        return (
          firstButton.getRootNode().activeElement !== firstButton &&
          lastButton.getRootNode().activeElement !== lastButton
        );
      }, "The first and last buttons do not have focus");
      Assert.notEqual(
        Services.focus.focusedElement,
        firstButton,
        "The first button does not have focus"
      );
      Assert.notEqual(
        Services.focus.focusedElement,
        lastButton,
        "The last button does not have focus"
      );

      EventUtils.synthesizeKey("KEY_Tab");

      info(
        `Actual focused id: ${Services.focus.focusedElement.id}. Expected focused id: ${firstButton.id}`
      );
      await BrowserTestUtils.waitForCondition(() => {
        return firstButton.getRootNode().activeElement === firstButton;
      }, "The first button in the panel should have focus");
      info(
        `Actual focused id: ${Services.focus.focusedElement.id}. Expected focused id: ${firstButton.id}`
      );
      is(
        Services.focus.focusedElement,
        firstButton,
        "The first button in the panel should have focus"
      );
    }
  );
});
