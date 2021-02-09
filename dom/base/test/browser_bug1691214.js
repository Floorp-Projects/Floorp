/* -*- Mode: JavaScript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const BASE_URL = "http://mochi.test:8888/browser/dom/base/test/";

add_task(async function() {
  await BrowserTestUtils.withNewTab(
    BASE_URL + "file_bug1691214.html",
    async function(browser) {
      let win;

      {
        let newWindow = BrowserTestUtils.domWindowOpenedAndLoaded();
        let delayedStartupPromise = BrowserTestUtils.waitForNewWindow();

        await BrowserTestUtils.synthesizeMouseAtCenter("#link-1", {}, browser);
        win = await newWindow;
        ok(win, "First navigation should've opened the new window");
        // New windows get focused after the first paint, see bug 1262946
        await BrowserTestUtils.waitForContentEvent(
          win.gBrowser.selectedBrowser,
          "MozAfterPaint"
        );
        await delayedStartupPromise;
        is(Services.focus.focusedWindow, win, "New window should be focused");
      }

      {
        let focusBack = BrowserTestUtils.waitForEvent(window, "focus", true);
        window.focus();
        await focusBack;
        is(Services.focus.focusedWindow, window, "should focus back");
      }

      {
        let focus = BrowserTestUtils.waitForEvent(win, "focus", true);
        await BrowserTestUtils.synthesizeMouseAtCenter("#link-2", {}, browser);
        await focus;
      }

      is(
        Services.focus.focusedWindow,
        win,
        "Existing window should've been targeted and focused"
      );

      win.close();
    }
  );
});
