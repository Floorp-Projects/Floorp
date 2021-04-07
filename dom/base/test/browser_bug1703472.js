/* -*- Mode: JavaScript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const BASE_URL = "http://mochi.test:8888/browser/dom/base/test/";

add_task(async function bug1703472() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.block_multiple_popups", true],
      ["dom.disable_open_during_load", true],
    ],
  });

  await BrowserTestUtils.withNewTab(
    BASE_URL + "file_bug1703472.html",
    async function(browser) {
      info("Opening popup");
      let win = await newFocusedWindow(function() {
        return BrowserTestUtils.synthesizeMouseAtCenter(
          "#openWindow",
          {},
          browser
        );
      });

      info("re-focusing the original window");
      {
        let focusBack = BrowserTestUtils.waitForEvent(window, "focus", true);
        window.focus();
        await focusBack;
        is(Services.focus.focusedWindow, window, "should focus back");
      }

      // The click to do window.open() should've consumed the user interaction,
      // and an artificial .click() shouldn't count as a user interaction, so the
      // page shouldn't be allowed to focus it again without user interaction.
      info("Trying to steal focus without interaction");
      await SpecialPowers.spawn(browser, [], function() {
        content.document.querySelector("#focusWindow").click();
      });

      // We need to wait for something _not_ happening, so we need to use an arbitrary setTimeout.
      await new Promise(resolve => {
        // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
        setTimeout(resolve, 500);
      });

      is(Services.focus.focusedWindow, window, "should still be focused");

      info("Trying to move focus with user interaction");
      {
        let focus = BrowserTestUtils.waitForEvent(win, "focus", true);
        await BrowserTestUtils.synthesizeMouseAtCenter(
          "#focusWindow",
          {},
          browser
        );
        await focus;
        is(Services.focus.focusedWindow, win, "should focus back");
      }

      win.close();
    }
  );
});
