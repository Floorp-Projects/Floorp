/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests for the caret position after gURLBar.setURI().
add_task(async function setURI() {
  const testData = [
    {
      firstURL: "https://example.com/test",
      secondURL: "https://example.com/test",
      initialSelectionStart: 0,
      initialSelectionEnd: 0,
      expectedSelectionStart: 0,
      expectedSelectionEnd: 0,
    },
    {
      firstURL: "https://example.com/test",
      secondURL: "https://example.com/test",
      initialSelectionStart: 20,
      initialSelectionEnd: 20,
      expectedSelectionStart: 20,
      expectedSelectionEnd: 20,
    },
    {
      firstURL: "https://example.com/test",
      secondURL: "https://example.com/test",
      initialSelectionStart: 1,
      initialSelectionEnd: 20,
      expectedSelectionStart: 1,
      expectedSelectionEnd: 20,
    },
    {
      firstURL: "https://example.com/test",
      secondURL: "https://example.com/test",
      initialSelectionStart: "https://example.com/test".length,
      initialSelectionEnd: "https://example.com/test".length,
      expectedSelectionStart: "https://example.com/test".length,
      expectedSelectionEnd: "https://example.com/test".length,
    },
    {
      firstURL: "https://example.com/test",
      secondURL: "https://example.com/test",
      initialSelectionStart: 0,
      initialSelectionEnd: "https://example.com/test".length,
      expectedSelectionStart: 0,
      expectedSelectionEnd: "https://example.com/test".length,
    },
    {
      firstURL: "https://example.com/test",
      secondURL: "https://example.org/test",
      initialSelectionStart: 0,
      initialSelectionEnd: 0,
      expectedSelectionStart: 0,
      expectedSelectionEnd: 0,
    },
    {
      firstURL: "https://example.com/test",
      secondURL: "https://example.org/test",
      initialSelectionStart: 20,
      initialSelectionEnd: 20,
      expectedSelectionStart: 20,
      expectedSelectionEnd: 20,
    },
    {
      firstURL: "https://example.com/test",
      secondURL: "https://example.org/test",
      initialSelectionStart: 1,
      initialSelectionEnd: 20,
      expectedSelectionStart: 1,
      expectedSelectionEnd: 20,
    },
    {
      firstURL: "https://example.com/test",
      secondURL: "https://example.org/test",
      initialSelectionStart: "https://example.com/test".length,
      initialSelectionEnd: "https://example.com/test".length,
      expectedSelectionStart: "https://example.org/test".length,
      expectedSelectionEnd: "https://example.org/test".length,
    },
    {
      firstURL: "https://example.com/test",
      secondURL: "https://example.org/test",
      initialSelectionStart: 0,
      initialSelectionEnd: "https://example.com/test".length,
      expectedSelectionStart: "https://example.org/test".length,
      expectedSelectionEnd: "https://example.org/test".length,
    },
    {
      firstURL: "https://example.com/test",
      secondURL: "https://example.com/longer",
      initialSelectionStart: "https://example.com/test".length,
      initialSelectionEnd: "https://example.com/test".length,
      expectedSelectionStart: "https://example.com/longer".length,
      expectedSelectionEnd: "https://example.com/longer".length,
    },
    {
      firstURL: "https://example.com/test",
      secondURL: "https://example.com/longer",
      initialSelectionStart: 20,
      initialSelectionEnd: 20,
      expectedSelectionStart: 20,
      expectedSelectionEnd: 20,
    },
    {
      firstURL: "https://example.com/longer",
      secondURL: "https://example.com/test",
      initialSelectionStart: 0,
      initialSelectionEnd: "https://example.com/longer".length,
      expectedSelectionStart: "https://example.com/test".length,
      expectedSelectionEnd: "https://example.com/test".length,
    },
    {
      firstURL: "https://example.com/longer",
      secondURL: "https://example.com/test",
      initialSelectionStart: "https://example.com/longer".length,
      initialSelectionEnd: "https://example.com/longer".length,
      expectedSelectionStart: "https://example.com/test".length,
      expectedSelectionEnd: "https://example.com/test".length,
    },
    {
      firstURL: "https://example.com/longer",
      secondURL: "https://example.com/test",
      initialSelectionStart: "https://example.com/longer".length - 1,
      initialSelectionEnd: "https://example.com/longer".length - 1,
      expectedSelectionStart: "https://example.com/test".length,
      expectedSelectionEnd: "https://example.com/test".length,
    },
    {
      firstURL: "https://example.com/longer",
      secondURL: "https://example.com/test",
      initialSelectionStart: 0,
      initialSelectionEnd: "https://example.com/longer".length - 1,
      expectedSelectionStart: "https://example.com/test".length,
      expectedSelectionEnd: "https://example.com/test".length,
    },
  ];

  for (const data of testData) {
    info(
      `Test for ${data.firstURL} -> ${data.secondURL} with initial selection: ${data.initialSelectionStart}, ${data.initialSelectionEnd}`
    );

    gURLBar.setURI(makeURI(data.firstURL));

    gURLBar.selectionStart = data.initialSelectionStart;
    gURLBar.selectionEnd = data.initialSelectionEnd;

    gURLBar.setURI(makeURI(data.secondURL));

    Assert.equal(gURLBar.selectionStart, data.expectedSelectionStart);
    Assert.equal(gURLBar.selectionEnd, data.expectedSelectionEnd);
  }
});

// Tests that up and down keys move the caret on certain platforms, and that
// opening the popup doesn't change the caret position.
add_task(async function navigation() {
  // Use new window to avoid timeout failure for autocomplete popup happens on Linux TV.
  const win = await BrowserTestUtils.openNewBrowserWindow();

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: win,
    value: "This is a generic sentence",
  });
  await UrlbarTestUtils.promisePopupClose(win);

  const INITIAL_SELECTION_START = 3;
  const INITIAL_SELECTION_END = 10;
  win.gURLBar.selectionStart = INITIAL_SELECTION_START;
  win.gURLBar.selectionEnd = INITIAL_SELECTION_END;

  if (AppConstants.platform == "macosx" || AppConstants.platform == "linux") {
    await checkCaretMoves(
      "KEY_ArrowDown",
      win.gURLBar.value.length,
      "Caret should have moved to the end",
      win
    );
    await checkPopupOpens("KEY_ArrowDown", win);

    await checkCaretMoves(
      "KEY_ArrowUp",
      0,
      "Caret should have moved to the start",
      win
    );
    await checkPopupOpens("KEY_ArrowUp", win);
  } else {
    await checkPopupOpens("KEY_ArrowDown", win);
    await checkPopupOpens("KEY_ArrowUp", win);
  }

  await BrowserTestUtils.closeWindow(win);
});

async function checkCaretMoves(key, pos, msg, win) {
  checkIfKeyStartsQuery(key, false, win);
  Assert.equal(
    UrlbarTestUtils.isPopupOpen(win),
    false,
    `${key}: Popup shouldn't be open`
  );
  Assert.equal(
    win.gURLBar.selectionStart,
    win.gURLBar.selectionEnd,
    `${key}: Input selection should be empty`
  );
  Assert.equal(win.gURLBar.selectionStart, pos, `${key}: ${msg}`);
}

async function checkPopupOpens(key, win) {
  // Store current selection and check it doesn't change.
  let selectionStart = win.gURLBar.selectionStart;
  let selectionEnd = win.gURLBar.selectionEnd;
  await UrlbarTestUtils.promisePopupOpen(win, () => {
    checkIfKeyStartsQuery(key, true, win);
  });
  Assert.equal(
    UrlbarTestUtils.getSelectedRowIndex(win),
    0,
    `${key}: Heuristic result should be selected`
  );
  Assert.equal(
    win.gURLBar.selectionStart,
    selectionStart,
    `${key}: Input selection start should not change`
  );
  Assert.equal(
    win.gURLBar.selectionEnd,
    selectionEnd,
    `${key}: Input selection end should not change`
  );
  await UrlbarTestUtils.promisePopupClose(win);
}

function checkIfKeyStartsQuery(key, shouldStartQuery, win) {
  let queryStarted = false;
  let queryListener = {
    onQueryStarted() {
      queryStarted = true;
    },
  };
  win.gURLBar.controller.addQueryListener(queryListener);
  EventUtils.synthesizeKey(key, {}, win);
  win.gURLBar.eventBufferer.replayDeferredEvents(false);
  win.gURLBar.controller.removeQueryListener(queryListener);
  Assert.equal(
    queryStarted,
    shouldStartQuery,
    `${key}: Should${shouldStartQuery ? "" : "n't"} have started a query`
  );
}
