/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Caret move events checker.
 */
function caretMoveChecker(target, caretOffset) {
  return function(event) {
    let cmEvent = event.QueryInterface(nsIAccessibleCaretMoveEvent);
    return cmEvent.accessible == getAccessible(target) && cmEvent.caretOffset == caretOffset;
  };
}

async function checkURLBarCaretEvents() {
  let url = "about:mozilla";

  let onDocLoad = waitForEvent(
    EVENT_DOCUMENT_LOAD_COMPLETE,
    event => {
      try {
        return event.accessible.QueryInterface(nsIAccessibleDocument).URL == url;
      } catch (e) {
        return false;
      }
    }
  );
  let [ newWin ] = await Promise.all([
    BrowserTestUtils.openNewBrowserWindow({ url }),
    onDocLoad
  ]);

  let urlbarInputEl = newWin.document.getElementById("urlbar").inputField;
  let urlbarInput = getAccessible(urlbarInputEl, [ nsIAccessibleText ]);

  let onCaretMove = waitForEvents([
    [ EVENT_TEXT_CARET_MOVED, caretMoveChecker(urlbarInput, url.length) ],
    [ EVENT_FOCUS, urlbarInput ]
  ]);

  urlbarInput.caretOffset = -1;
  await onCaretMove;
  ok(true, "Caret move in URL bar #1");

  onCaretMove = waitForEvent(
    EVENT_TEXT_CARET_MOVED, caretMoveChecker(urlbarInput, 0)
  );

  urlbarInput.caretOffset = 0;
  await onCaretMove;
  ok(true, "Caret move in URL bar #2");

  await BrowserTestUtils.closeWindow(newWin);
}

add_task(checkURLBarCaretEvents);
