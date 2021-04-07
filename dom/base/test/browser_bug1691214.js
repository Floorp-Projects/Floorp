/* -*- Mode: JavaScript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const BASE_URL = "http://mochi.test:8888/browser/dom/base/test/";

add_task(async function bug1691214() {
  await BrowserTestUtils.withNewTab(
    BASE_URL + "file_bug1691214.html",
    async function(browser) {
      let win = await newFocusedWindow(function() {
        return BrowserTestUtils.synthesizeMouseAtCenter("#link-1", {}, browser);
      });
      is(Services.focus.focusedWindow, win, "New window should be focused");

      info("re-focusing the original window");

      {
        let focusBack = BrowserTestUtils.waitForEvent(window, "focus", true);
        window.focus();
        await focusBack;
        is(Services.focus.focusedWindow, window, "should focus back");
      }

      info("Clicking on the second link.");

      {
        let focus = BrowserTestUtils.waitForEvent(win, "focus", true);
        await BrowserTestUtils.synthesizeMouseAtCenter("#link-2", {}, browser);
        info("Waiting for re-focus of the opened window.");
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

// The tab has a form infinitely submitting to an iframe, and that shouldn't
// switch focus back. For that, we open a window from the tab, make sure it's
// focused, and then wait for three submissions (but since we need to listen to
// trusted events from chrome code, we use the iframe load event instead).
add_task(async function bug1700871() {
  await BrowserTestUtils.withNewTab(
    BASE_URL + "file_bug1700871.html",
    async function(browser) {
      let win = await newFocusedWindow(function() {
        return BrowserTestUtils.synthesizeMouseAtCenter("#link-1", {}, browser);
      });

      is(Services.focus.focusedWindow, win, "New window should be focused");

      info("waiting for three submit events and ensuring focus hasn't moved");

      await SpecialPowers.spawn(browser, [], function() {
        let pending = 3;
        return new Promise(resolve => {
          content.document
            .querySelector("iframe")
            .addEventListener("load", function(e) {
              info("Got load on the frame: " + pending);
              if (!--pending) {
                resolve();
              }
            });
        });
      });

      is(Services.focus.focusedWindow, win, "Focus shouldn't have moved");
      win.close();
    }
  );
});
