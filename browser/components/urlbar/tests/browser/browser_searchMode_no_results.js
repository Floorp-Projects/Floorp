/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests entering search mode and there are no results in the view.
 */

"use strict";

const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.update2", true],
      ["browser.urlbar.update2.localOneOffs", true],
      ["browser.urlbar.update2.oneOffsRefresh", true],
    ],
  });

  // In order to open the view without any results, we need to be in search mode
  // with an empty search string so that no heuristic result is shown, and the
  // empty search must yield zero additional results.  We'll enter search mode
  // using the bookmarks one-off, and first we'll delete all bookmarks so that
  // there are no results.
  await PlacesUtils.bookmarks.eraseEverything();

  // Also clear history so that using the alias of our test engine doesn't
  // inadvertently return any history results due to bug 1658646.
  await PlacesUtils.history.clear();

  // Add a top site so we're guaranteed the view has at least one result to
  // show initially with an empty search.  Otherwise the view won't even open.
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.newtabpage.activity-stream.default.sites",
        "http://example.com/",
      ],
    ],
  });
  await updateTopSites(sites => sites.length);
});

// Basic test for entering search mode with no results.
add_task(async function basic() {
  await withNewWindow(async win => {
    // Do an empty search.
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      value: "",
    });

    // Initially there should be at least the top site we added above.
    Assert.greater(
      UrlbarTestUtils.getResultCount(win),
      0,
      "Top sites should be present initially"
    );
    Assert.ok(
      !win.gURLBar.panel.hasAttribute("noresults"),
      "Panel has results, therefore should not have noresults attribute"
    );

    // Enter search mode by clicking the bookmarks one-off.
    await UrlbarTestUtils.enterSearchMode(win, {
      source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
    });

    Assert.equal(
      UrlbarTestUtils.getResultCount(win),
      0,
      "Zero results since no bookmarks exist"
    );
    Assert.equal(
      win.gURLBar.panel.getAttribute("noresults"),
      "true",
      "Panel has no results, therefore should have noresults attribute"
    );

    // Exit search mode by backspacing.  The top sites should be shown again.
    await UrlbarTestUtils.exitSearchMode(win, { backspace: true });
    Assert.greater(
      UrlbarTestUtils.getResultCount(win),
      0,
      "Top sites should be present again"
    );
    Assert.ok(
      !win.gURLBar.panel.hasAttribute("noresults"),
      "noresults attribute should be absent again"
    );

    await UrlbarTestUtils.promisePopupClose(win);
  });
});

// When the urlbar is in search mode, has no results, and is not focused,
// focusing it should auto-open the view.
add_task(async function autoOpen() {
  await withNewWindow(async win => {
    // Do an empty search.
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      value: "",
    });

    // Initially there should be at least the top site we added above.
    Assert.greater(
      UrlbarTestUtils.getResultCount(win),
      0,
      "Top sites should be present initially"
    );
    Assert.ok(
      !win.gURLBar.panel.hasAttribute("noresults"),
      "Panel has results, therefore should not have noresults attribute"
    );

    // Enter search mode by clicking the bookmarks one-off.
    await UrlbarTestUtils.enterSearchMode(win, {
      source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
    });
    Assert.equal(
      UrlbarTestUtils.getResultCount(win),
      0,
      "Zero results since no bookmarks exist"
    );
    Assert.equal(
      win.gURLBar.panel.getAttribute("noresults"),
      "true",
      "Panel has no results, therefore should have noresults attribute"
    );

    // Blur the urlbar.
    win.gURLBar.blur();
    await UrlbarTestUtils.assertSearchMode(win, {
      source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
    });

    // Click the urlbar.
    await UrlbarTestUtils.promisePopupOpen(win, () => {
      EventUtils.synthesizeMouseAtCenter(win.gURLBar.inputField, {}, win);
    });
    Assert.equal(
      UrlbarTestUtils.getResultCount(win),
      0,
      "Still zero results since no bookmarks exist"
    );
    Assert.equal(
      win.gURLBar.panel.getAttribute("noresults"),
      "true",
      "Panel still has no results, therefore should have noresults attribute"
    );

    // Exit search mode by backspacing.  The top sites should be shown again.
    await UrlbarTestUtils.exitSearchMode(win, { backspace: true });
    Assert.greater(
      UrlbarTestUtils.getResultCount(win),
      0,
      "Top sites should be present again"
    );
    Assert.ok(
      !win.gURLBar.panel.hasAttribute("noresults"),
      "noresults attribute should be absent again"
    );

    await UrlbarTestUtils.promisePopupClose(win);
  });
});

// When the urlbar is in search mode, the user backspaces over the final char
// (but remains in search mode), and there are no results, the view should
// remain open.
add_task(async function backspaceRemainOpen() {
  await withNewWindow(async win => {
    // Do a one-char search.
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      value: "x",
    });
    Assert.greater(
      UrlbarTestUtils.getResultCount(win),
      0,
      "At least the heuristic result should be shown"
    );
    Assert.ok(
      !win.gURLBar.panel.hasAttribute("noresults"),
      "Panel has results, therefore should not have noresults attribute"
    );

    // Enter search mode by clicking the bookmarks one-off.
    await UrlbarTestUtils.enterSearchMode(win, {
      source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
    });

    // The heursitic should now be shown since we always show it in search mode
    // when the search string is *not* empty, even when there are no other
    // results.
    Assert.greater(
      UrlbarTestUtils.getResultCount(win),
      0,
      "At least the heuristic result should be shown"
    );
    Assert.ok(
      !win.gURLBar.panel.hasAttribute("noresults"),
      "Panel has results, therefore should not have noresults attribute"
    );

    // Backspace.  The search string will now be empty.
    let searchPromise = UrlbarTestUtils.promiseSearchComplete(win);
    EventUtils.synthesizeKey("KEY_Backspace", {}, win);
    await searchPromise;
    Assert.ok(UrlbarTestUtils.isPopupOpen(win), "View remains open");
    await UrlbarTestUtils.assertSearchMode(win, {
      source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
    });
    Assert.equal(
      UrlbarTestUtils.getResultCount(win),
      0,
      "Zero results since no bookmarks exist"
    );
    Assert.equal(
      win.gURLBar.panel.getAttribute("noresults"),
      "true",
      "Panel has no results, therefore should have noresults attribute"
    );

    // Exit search mode by backspacing.  The top sites should be shown.
    await UrlbarTestUtils.exitSearchMode(win, { backspace: true });
    Assert.greater(
      UrlbarTestUtils.getResultCount(win),
      0,
      "Top sites should be present again"
    );
    Assert.ok(
      !win.gURLBar.panel.hasAttribute("noresults"),
      "noresults attribute should be absent again"
    );

    await UrlbarTestUtils.promisePopupClose(win);
  });
});

// Types a search alias and then a space to enter search mode, with no results.
// The one-offs should be shown.
add_task(async function spaceToEnterSearchMode() {
  let engine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME
  );
  engine.alias = "@test";

  await withNewWindow(async win => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      value: engine.alias,
    });

    let searchPromise = UrlbarTestUtils.promiseSearchComplete(win);
    EventUtils.synthesizeKey(" ", {}, win);
    await searchPromise;

    Assert.equal(UrlbarTestUtils.getResultCount(win), 0, "Zero results");
    Assert.equal(
      win.gURLBar.panel.getAttribute("noresults"),
      "true",
      "Panel has no results, therefore should have noresults attribute"
    );
    await UrlbarTestUtils.assertSearchMode(win, {
      engineName: engine.name,
    });
    this.Assert.equal(
      UrlbarTestUtils.getOneOffSearchButtonsVisible(window),
      true,
      "One-offs are visible"
    );

    await UrlbarTestUtils.exitSearchMode(win, { backspace: true });
    await UrlbarTestUtils.promisePopupClose(win);
  });
});

/**
 * Opens a new window, waits for it to load, calls a callback, and closes the
 * window.  We use a new window in each task so that the view starts with a
 * blank slate each time.
 *
 * @param {function} callback
 *   Will be called as: callback(newWindow)
 */
async function withNewWindow(callback) {
  // Start in a new window so we have a blank slate.
  let win = await BrowserTestUtils.openNewBrowserWindow();
  await callback(win);
  await BrowserTestUtils.closeWindow(win);
}
