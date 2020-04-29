"use strict";

/**
 * These tests are for the auto-focus behaviour on the initial browser
 * when a window is opened from content.
 */

const PAGE = `data:text/html,<a id="target" href="%23" onclick="window.open('http://www.example.com', '_blank', 'width=100,height=100');">Click me</a>`;

/**
 * Test that when a new window is opened from content, focus moves
 * to the initial browser in that window once the window has finished
 * painting.
 */
add_task(async function test_focus_browser() {
  await BrowserTestUtils.withNewTab(
    {
      url: PAGE,
      gBrowser,
    },
    async function(browser) {
      let newWinPromise = BrowserTestUtils.domWindowOpenedAndLoaded(null);
      let delayedStartupPromise = BrowserTestUtils.waitForNewWindow();

      await BrowserTestUtils.synthesizeMouseAtCenter("#target", {}, browser);
      let newWin = await newWinPromise;
      await BrowserTestUtils.waitForContentEvent(
        newWin.gBrowser.selectedBrowser,
        "MozAfterPaint"
      );
      await delayedStartupPromise;

      let focusedElement = Services.focus.getFocusedElementForWindow(
        newWin,
        false,
        {}
      );

      Assert.equal(
        focusedElement,
        newWin.gBrowser.selectedBrowser,
        "Initial browser should be focused"
      );

      await BrowserTestUtils.closeWindow(newWin);
    }
  );
});

/**
 * Test that when a new window is opened from content and focus
 * shifts in that window before the content has a chance to paint
 * that we _don't_ steal focus once content has painted.
 */
add_task(async function test_no_steal_focus() {
  await BrowserTestUtils.withNewTab(
    {
      url: PAGE,
      gBrowser,
    },
    async function(browser) {
      let newWinPromise = BrowserTestUtils.domWindowOpenedAndLoaded(null);
      let delayedStartupPromise = BrowserTestUtils.waitForNewWindow();

      await BrowserTestUtils.synthesizeMouseAtCenter("#target", {}, browser);
      let newWin = await newWinPromise;

      // Because we're switching focus, we shouldn't steal it once
      // content paints.
      newWin.gURLBar.focus();

      await BrowserTestUtils.waitForContentEvent(
        newWin.gBrowser.selectedBrowser,
        "MozAfterPaint"
      );
      await delayedStartupPromise;

      let focusedElement = Services.focus.getFocusedElementForWindow(
        newWin,
        false,
        {}
      );

      Assert.equal(
        focusedElement,
        newWin.gURLBar.inputField,
        "URLBar should be focused"
      );

      await BrowserTestUtils.closeWindow(newWin);
    }
  );
});
