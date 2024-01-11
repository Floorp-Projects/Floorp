/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

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
        // id, options, result
        ["#instant", normalEvent, "tab"],
        ["#instant", shiftEvent, "window"],
        ["#instant", metaEvent, "tab-bg"],
        ["#instant", metaShiftEvent, "tab"],

        ["#instant-popup", normalEvent, "popup"],
        ["#instant-popup", shiftEvent, "window"],
        ["#instant-popup", metaEvent, "tab-bg"],
        ["#instant-popup", metaShiftEvent, "tab"],

        ["#delayed", normalEvent, "tab"],
        ["#delayed", shiftEvent, "window"],
        ["#delayed", metaEvent, "tab-bg"],
        ["#delayed", metaShiftEvent, "tab"],

        ["#delayed-popup", normalEvent, "popup"],
        ["#delayed-popup", shiftEvent, "window"],
        ["#delayed-popup", metaEvent, "tab-bg"],
        ["#delayed-popup", metaShiftEvent, "tab"],
      ];

      for (const [id, event, result] of tests) {
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

        BrowserTestUtils.synthesizeMouseAtCenter(id, { ...event }, browser);

        const openedThing = await openPromise;

        if (result == "tab" || result == "tab-bg") {
          const newTab = openedThing;

          if (result == "tab") {
            ok(
              gBrowser.selectedTab == newTab,
              `${id} with ${eventStr} opened a foreground tab`
            );
          } else {
            ok(
              gBrowser.selectedTab != newTab,
              `${id} with ${eventStr} opened a background tab`
            );
          }

          gBrowser.removeTab(newTab);
        } else {
          const newWindow = openedThing;

          const tabs = newWindow.document.getElementById("TabsToolbar");
          if (result == "window") {
            ok(
              !tabs.collapsed,
              `${id} with ${eventStr} opened a regular window`
            );
          } else {
            ok(tabs.collapsed, `${id} with ${eventStr} opened a popup window`);
          }

          const closedPopupPromise = BrowserTestUtils.windowClosed(newWindow);
          newWindow.close();
          await closedPopupPromise;
        }
      }
    }
  );
});
