/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Opening many windows take long time on some configuration.
requestLongerTimeout(6);

const TEST_URL =
  "https://example.com/browser/browser/base/content/test/tabs/file_window_open.html";

const metaKey = AppConstants.platform == "macosx" ? "metaKey" : "ctrlKey";

const normalEvent = {};
const shiftEvent = { shiftKey: true };
const metaEvent = { [metaKey]: true };
const metaShiftEvent = { [metaKey]: true, shiftKey: true };

const altEvent = { altKey: true };
const altShiftEvent = { altKey: true, shiftKey: true };
const altMetaEvent = { altKey: true, [metaKey]: true };
const altMetaShiftEvent = { altKey: true, [metaKey]: true, shiftKey: true };

const middleEvent = { button: 1 };
const middleShiftEvent = { button: 1, shiftKey: true };
const middleMetaEvent = { button: 1, [metaKey]: true };
const middleMetaShiftEvent = { button: 1, [metaKey]: true, shiftKey: true };

add_task(async function testMouse() {
  await BrowserTestUtils.withNewTab(TEST_URL, async function (browser) {
    const tests = [
      // type, id, options, result
      ["mouse", "#instant", normalEvent, "tab"],
      ["mouse", "#instant", shiftEvent, "window"],
      ["mouse", "#instant", metaEvent, "tab-bg"],
      ["mouse", "#instant", metaShiftEvent, "tab"],

      ["mouse", "#instant-popup", normalEvent, "popup"],
      ["mouse", "#instant-popup", shiftEvent, "window"],
      ["mouse", "#instant-popup", metaEvent, "tab-bg"],
      ["mouse", "#instant-popup", metaShiftEvent, "tab"],

      ["mouse", "#delayed", normalEvent, "tab"],
      ["mouse", "#delayed", shiftEvent, "window"],
      ["mouse", "#delayed", metaEvent, "tab-bg"],
      ["mouse", "#delayed", metaShiftEvent, "tab"],

      ["mouse", "#delayed-popup", normalEvent, "popup"],
      ["mouse", "#delayed-popup", shiftEvent, "window"],
      ["mouse", "#delayed-popup", metaEvent, "tab-bg"],
      ["mouse", "#delayed-popup", metaShiftEvent, "tab"],
    ];
    await runWindowOpenTests(browser, tests);
  });
});

add_task(async function testAlt() {
  // Alt key shouldn't affect the behavior.
  await BrowserTestUtils.withNewTab(TEST_URL, async function (browser) {
    const tests = [
      ["mouse", "#instant", altEvent, "tab"],
      ["mouse", "#instant", altShiftEvent, "window"],
      ["mouse", "#instant", altMetaEvent, "tab-bg"],
      ["mouse", "#instant", altMetaShiftEvent, "tab"],

      ["mouse", "#instant-popup", altEvent, "popup"],
      ["mouse", "#instant-popup", altShiftEvent, "window"],
      ["mouse", "#instant-popup", altMetaEvent, "tab-bg"],
      ["mouse", "#instant-popup", altMetaShiftEvent, "tab"],

      ["mouse", "#delayed", altEvent, "tab"],
      ["mouse", "#delayed", altShiftEvent, "window"],
      ["mouse", "#delayed", altMetaEvent, "tab-bg"],
      ["mouse", "#delayed", altMetaShiftEvent, "tab"],

      ["mouse", "#delayed-popup", altEvent, "popup"],
      ["mouse", "#delayed-popup", altShiftEvent, "window"],
      ["mouse", "#delayed-popup", altMetaEvent, "tab-bg"],
      ["mouse", "#delayed-popup", altMetaShiftEvent, "tab"],
    ];
    await runWindowOpenTests(browser, tests);
  });
});

add_task(async function testMiddleMouse() {
  // Middle click is equivalent to meta key.
  await BrowserTestUtils.withNewTab(TEST_URL, async function (browser) {
    const tests = [
      ["mouse", "#instant", middleEvent, "tab-bg"],
      ["mouse", "#instant", middleShiftEvent, "tab"],
      ["mouse", "#instant", middleMetaEvent, "tab-bg"],
      ["mouse", "#instant", middleMetaShiftEvent, "tab"],

      ["mouse", "#instant-popup", middleEvent, "tab-bg"],
      ["mouse", "#instant-popup", middleShiftEvent, "tab"],
      ["mouse", "#instant-popup", middleMetaEvent, "tab-bg"],
      ["mouse", "#instant-popup", middleMetaShiftEvent, "tab"],

      ["mouse", "#delayed", middleEvent, "tab-bg"],
      ["mouse", "#delayed", middleShiftEvent, "tab"],
      ["mouse", "#delayed", middleMetaEvent, "tab-bg"],
      ["mouse", "#delayed", middleMetaShiftEvent, "tab"],

      ["mouse", "#delayed-popup", middleEvent, "tab-bg"],
      ["mouse", "#delayed-popup", middleShiftEvent, "tab"],
      ["mouse", "#delayed-popup", middleMetaEvent, "tab-bg"],
      ["mouse", "#delayed-popup", middleMetaShiftEvent, "tab"],
    ];
    await runWindowOpenTests(browser, tests);
  });
});

add_task(async function testBackgroundPrefTests() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.loadInBackground", false]],
  });

  await BrowserTestUtils.withNewTab(TEST_URL, async function (browser) {
    const tests = [
      ["mouse", "#instant", metaEvent, "tab"],
      ["mouse", "#instant", metaShiftEvent, "tab-bg"],
    ];
    await runWindowOpenTests(browser, tests);
  });

  await SpecialPowers.popPrefEnv();
});

add_task(async function testSpaceKey() {
  await BrowserTestUtils.withNewTab(TEST_URL, async function (browser) {
    const tests = [
      // NOTE: meta+keyboard doesn't activate.

      ["VK_SPACE", "#instant", normalEvent, "tab"],
      ["VK_SPACE", "#instant", shiftEvent, "window"],

      ["VK_SPACE", "#instant-popup", normalEvent, "popup"],
      ["VK_SPACE", "#instant-popup", shiftEvent, "window"],

      ["VK_SPACE", "#delayed", normalEvent, "tab"],
      ["VK_SPACE", "#delayed", shiftEvent, "window"],

      ["VK_SPACE", "#delayed-popup", normalEvent, "popup"],
      ["VK_SPACE", "#delayed-popup", shiftEvent, "window"],
    ];
    await runWindowOpenTests(browser, tests);
  });
});

add_task(async function testEnterKey() {
  await BrowserTestUtils.withNewTab(TEST_URL, async function (browser) {
    const tests = [
      ["KEY_Enter", "#link-instant", normalEvent, "tab"],
      ["KEY_Enter", "#link-instant", shiftEvent, "window"],

      ["KEY_Enter", "#link-instant-popup", normalEvent, "popup"],
      ["KEY_Enter", "#link-instant-popup", shiftEvent, "window"],

      ["KEY_Enter", "#link-delayed", normalEvent, "tab"],
      ["KEY_Enter", "#link-delayed", shiftEvent, "window"],

      ["KEY_Enter", "#link-delayed-popup", normalEvent, "popup"],
      ["KEY_Enter", "#link-delayed-popup", shiftEvent, "window"],
    ];
    await runWindowOpenTests(browser, tests);
  });
});

add_task(async function testUserDefinedShortcut() {
  await BrowserTestUtils.withNewTab(TEST_URL, async function (browser) {
    const tests = [
      // Trigger user-defined shortcut key, where modifiers shouldn't affect.

      ["x", "#instant", normalEvent, "tab"],
      ["x", "#instant", shiftEvent, "tab"],
      ["x", "#instant", metaEvent, "tab"],
      ["x", "#instant", metaShiftEvent, "tab"],

      ["y", "#instant", normalEvent, "popup"],
      ["y", "#instant", shiftEvent, "popup"],
      ["y", "#instant", metaEvent, "popup"],
      ["y", "#instant", metaShiftEvent, "popup"],
    ];
    await runWindowOpenTests(browser, tests);
  });
});

async function runWindowOpenTests(browser, tests) {
  for (const [type, id, event, result] of tests) {
    let eventStr = JSON.stringify(event);

    let openPromise;
    if (result == "tab" || result == "tab-bg") {
      openPromise = BrowserTestUtils.waitForNewTab(
        gBrowser,
        "about:blank",
        true
      );
    } else {
      openPromise = BrowserTestUtils.waitForNewWindow({
        url: "about:blank",
      });
    }

    if (type == "mouse") {
      BrowserTestUtils.synthesizeMouseAtCenter(id, { ...event }, browser);
    } else {
      // Make sure the keyboard activates a simple button on the page.
      await ContentTask.spawn(browser, id, elementId => {
        content.document.querySelector("#focus-result").value = "";
        content.document.querySelector("#focus-check").focus();
      });
      BrowserTestUtils.synthesizeKey("VK_SPACE", {}, browser);
      await ContentTask.spawn(browser, {}, async () => {
        await ContentTaskUtils.waitForCondition(
          () => content.document.querySelector("#focus-result").value === "ok"
        );
      });

      // Once confirmed the keyboard event works, send the actual event
      // that calls window.open.
      await ContentTask.spawn(browser, id, elementId => {
        content.document.querySelector(elementId).focus();
      });
      BrowserTestUtils.synthesizeKey(type, { ...event }, browser);
    }

    const openedThing = await openPromise;

    if (result == "tab" || result == "tab-bg") {
      const newTab = openedThing;

      if (result == "tab") {
        Assert.equal(
          gBrowser.selectedTab,
          newTab,
          `${id} with ${type} and ${eventStr} opened a foreground tab`
        );
      } else {
        Assert.notEqual(
          gBrowser.selectedTab,
          newTab,
          `${id} with ${type} and ${eventStr} opened a background tab`
        );
      }

      gBrowser.removeTab(newTab);
    } else {
      const newWindow = openedThing;

      const tabs = newWindow.document.getElementById("TabsToolbar");
      if (result == "window") {
        ok(
          !tabs.collapsed,
          `${id} with ${type} and ${eventStr} opened a regular window`
        );
      } else {
        ok(
          tabs.collapsed,
          `${id} with ${type} and ${eventStr} opened a popup window`
        );
      }

      const closedPopupPromise = BrowserTestUtils.windowClosed(newWindow);
      newWindow.close();
      await closedPopupPromise;

      // Make sure the focus comes back to this window before proceeding
      // to the next test.
      if (Services.focus.focusedWindow != window) {
        const focusBack = BrowserTestUtils.waitForEvent(window, "focus", true);
        window.focus();
        await focusBack;
      }
    }
  }
}
