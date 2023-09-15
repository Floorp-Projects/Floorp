/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Caret move events checker.
 */
function caretMoveChecker(target, caretOffset) {
  return function (event) {
    let cmEvent = event.QueryInterface(nsIAccessibleCaretMoveEvent);
    return (
      cmEvent.accessible == getAccessible(target) &&
      cmEvent.caretOffset == caretOffset
    );
  };
}

async function checkURLBarCaretEvents() {
  const kURL = "about:mozilla";
  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  BrowserTestUtils.startLoadingURIString(newWin.gBrowser.selectedBrowser, kURL);
  newWin.gBrowser.selectedBrowser.focus();

  await waitForEvent(EVENT_DOCUMENT_LOAD_COMPLETE, event => {
    try {
      return event.accessible.QueryInterface(nsIAccessibleDocument).URL == kURL;
    } catch (e) {
      return false;
    }
  });
  info("Loaded " + kURL);

  let urlbarInputEl = newWin.gURLBar.inputField;
  let urlbarInput = getAccessible(urlbarInputEl, [nsIAccessibleText]);

  let onCaretMove = waitForEvents([
    [EVENT_TEXT_CARET_MOVED, caretMoveChecker(urlbarInput, kURL.length)],
    [EVENT_FOCUS, urlbarInput],
  ]);

  urlbarInput.caretOffset = -1;
  await onCaretMove;
  ok(true, "Caret move in URL bar #1");

  onCaretMove = waitForEvent(
    EVENT_TEXT_CARET_MOVED,
    caretMoveChecker(urlbarInput, 0)
  );

  urlbarInput.caretOffset = 0;
  await onCaretMove;
  ok(true, "Caret move in URL bar #2");

  await BrowserTestUtils.closeWindow(newWin);
}

add_task(checkURLBarCaretEvents);
