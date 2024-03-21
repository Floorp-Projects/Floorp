/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Opening many windows take long time on some configuration.
requestLongerTimeout(4);

add_task(async function () {
  await BrowserTestUtils.withNewTab(
    "https://example.com/browser/browser/base/content/test/tabs/file_window_open.html",
    async function (browser) {
      const metaKey = AppConstants.platform == "macosx" ? "metaKey" : "ctrlKey";
      const normalEvent = {};
      const shiftEvent = { shiftKey: true };
      const metaEvent = { [metaKey]: true };
      const metaShiftEvent = { [metaKey]: true, shiftKey: true };

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

        // NOTE: meta+keyboard doesn't activate.

        ["VK_SPACE", "#instant", normalEvent, "tab"],
        ["VK_SPACE", "#instant", shiftEvent, "window"],

        ["VK_SPACE", "#instant-popup", normalEvent, "popup"],
        ["VK_SPACE", "#instant-popup", shiftEvent, "window"],

        ["VK_SPACE", "#delayed", normalEvent, "tab"],
        ["VK_SPACE", "#delayed", shiftEvent, "window"],

        ["VK_SPACE", "#delayed-popup", normalEvent, "popup"],
        ["VK_SPACE", "#delayed-popup", shiftEvent, "window"],

        ["KEY_Enter", "#link-instant", normalEvent, "tab"],
        ["KEY_Enter", "#link-instant", shiftEvent, "window"],

        ["KEY_Enter", "#link-instant-popup", normalEvent, "popup"],
        ["KEY_Enter", "#link-instant-popup", shiftEvent, "window"],

        ["KEY_Enter", "#link-delayed", normalEvent, "tab"],
        ["KEY_Enter", "#link-delayed", shiftEvent, "window"],

        ["KEY_Enter", "#link-delayed-popup", normalEvent, "popup"],
        ["KEY_Enter", "#link-delayed-popup", shiftEvent, "window"],

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
      for (const [type, id, event, result] of tests) {
        const eventStr = JSON.stringify(event);

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
          await ContentTask.spawn(browser, id, () => {
            content.document.querySelector("#focus-result").value = "";
            content.document.querySelector("#focus-check").focus();
          });
          BrowserTestUtils.synthesizeKey("VK_SPACE", {}, browser);
          await ContentTask.spawn(browser, {}, async () => {
            await ContentTaskUtils.waitForCondition(
              () =>
                content.document.querySelector("#focus-result").value === "ok"
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
            const focusBack = BrowserTestUtils.waitForEvent(
              window,
              "focus",
              true
            );
            window.focus();
            await focusBack;
          }
        }
      }
    }
  );
});
