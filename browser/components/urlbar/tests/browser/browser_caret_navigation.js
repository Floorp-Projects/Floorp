/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that up and down keys move the caret on certain platforms, and that
 * opening the popup doesn't change the caret position.
 */

add_task(async function() {
  await promiseAutocompleteResultPopup("This is a generic sentence");
  await UrlbarTestUtils.promisePopupClose(window);

  const INITIAL_SELECTION_START = 3;
  const INITIAL_SELECTION_END = 10;
  gURLBar.selectionStart = INITIAL_SELECTION_START;
  gURLBar.selectionEnd = INITIAL_SELECTION_END;

  if (AppConstants.platform == "macosx" ||
      AppConstants.platform == "linux") {
    if (AppConstants.platform == "linux") {
      await checkCaretMoves("KEY_ArrowUp", INITIAL_SELECTION_START, "Selection should be collapsed to its start");

      gURLBar.selectionStart = INITIAL_SELECTION_START;
      gURLBar.selectionEnd = INITIAL_SELECTION_END;
      await checkCaretMoves("KEY_ArrowDown", INITIAL_SELECTION_END, "Selection should be collapsed to its end");
    }

    await checkCaretMoves("KEY_ArrowDown", gURLBar.textValue.length, "Caret should have moved to the end");
    await checkPopupOpens("KEY_ArrowDown", gURLBar.textValue.length);

    await checkCaretMoves("KEY_ArrowUp", 0, "Caret should have moved to the start");
    await checkPopupOpens("KEY_ArrowUp", 0);
  } else {
    await checkPopupOpens("KEY_ArrowDown", gURLBar.textValue.length);
    await checkPopupOpens("KEY_ArrowUp", gURLBar.textValue.length);
  }
});

async function checkCaretMoves(key, pos, msg) {
  checkIfKeyStartsQuery(key, false);
  Assert.equal(UrlbarTestUtils.isPopupOpen(window), false, `${key}: Popup shouldn't be open`);
  Assert.equal(gURLBar.selectionStart, gURLBar.selectionEnd, `${key}: Input selection should be empty`);
  Assert.equal(gURLBar.selectionStart, pos, `${key}: ${msg}`);
}

async function checkPopupOpens(key, expectedCaretPosition) {
  await UrlbarTestUtils.promisePopupOpen(window, () => {
    checkIfKeyStartsQuery(key, true);
  });
  Assert.equal(UrlbarTestUtils.getSelectedIndex(window), 0, `${key}: Heuristic result should be selected`);
  Assert.equal(gURLBar.selectionStart, gURLBar.selectionEnd, `${key}: Input selection should be empty`);
  Assert.equal(gURLBar.selectionStart, expectedCaretPosition, `${key}: Caret is at the expected position`);
  await UrlbarTestUtils.promisePopupClose(window);
}

function checkIfKeyStartsQuery(key, shouldStartQuery) {
  let queryStarted = false;
  let queryListener = {
    onQueryStarted() {
      queryStarted = true;
    },
  };
  gURLBar.controller.addQueryListener(queryListener);
  EventUtils.synthesizeKey(key);
  gURLBar.eventBufferer.replayDeferredEvents(false);
  gURLBar.controller.removeQueryListener(queryListener);
  Assert.equal(queryStarted, shouldStartQuery,
               `${key}: Should${shouldStartQuery ? "" : "n't"} have started a query`);
}
