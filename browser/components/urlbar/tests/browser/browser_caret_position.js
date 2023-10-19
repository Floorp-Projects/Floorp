/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const LARGE_DATA_URL =
  "data:text/plain," + [...Array(1000)].map(() => "0123456789").join("");

// Tests for the caret position after gURLBar.setURI().
add_task(async function setURI() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.trimHttps", false]],
  });
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
      initialSelectionEnd: 10,
      expectedSelectionStart: 1,
      expectedSelectionEnd: 10,
    },
    {
      firstURL: "https://example.com/test",
      secondURL: "https://example.org/test",
      initialSelectionStart: "https://example.".length,
      initialSelectionEnd: "https://example.c".length,
      expectedSelectionStart: "https://example.c".length,
      expectedSelectionEnd: "https://example.c".length,
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
    {
      firstURL: "https://example.com/test",
      secondURL: "about:blank",
      initialSelectionStart: 0,
      initialSelectionEnd: 0,
      expectedSelectionStart: 0,
      expectedSelectionEnd: 0,
    },
    {
      firstURL: "https://example.com/test",
      secondURL: "about:blank",
      initialSelectionStart: 0,
      initialSelectionEnd: "https://example.com/test".length,
      expectedSelectionStart: 0,
      expectedSelectionEnd: 0,
    },
    {
      firstURL: "https://example.com/test",
      secondURL: "about:blank",
      initialSelectionStart: 3,
      initialSelectionEnd: 4,
      expectedSelectionStart: 0,
      expectedSelectionEnd: 0,
    },
    {
      firstURL: "https://example.com/test",
      secondURL: "about:blank",
      initialSelectionStart: "https://example.com/test".length,
      initialSelectionEnd: "https://example.com/test".length,
      expectedSelectionStart: 0,
      expectedSelectionEnd: 0,
    },
    {
      firstURL: "about:blank",
      secondURL: "https://example.com/test",
      initialSelectionStart: 0,
      initialSelectionEnd: 0,
      expectedSelectionStart: 0,
      expectedSelectionEnd: 0,
    },
    {
      firstURL: "about:blank",
      secondURL: LARGE_DATA_URL,
      initialSelectionStart: 0,
      initialSelectionEnd: 0,
      expectedSelectionStart: 0,
      expectedSelectionEnd: 0,
    },
    {
      firstURL: "about:telemetry",
      secondURL: LARGE_DATA_URL,
      initialSelectionStart: "about:telemetry".length,
      initialSelectionEnd: "about:telemetry".length,
      expectedSelectionStart: LARGE_DATA_URL.length,
      expectedSelectionEnd: LARGE_DATA_URL.length,
    },
  ];

  for (const data of testData) {
    info(
      `Test for ${data.firstURL} -> ${data.secondURL} with initial selection: ${data.initialSelectionStart}, ${data.initialSelectionEnd}`
    );
    info("Check the caret position after setting second URL");
    gURLBar.setURI(makeURI(data.firstURL));
    gURLBar.selectionStart = data.initialSelectionStart;
    gURLBar.selectionEnd = data.initialSelectionEnd;

    // The change of the scroll amount dependent on the selection change will be
    // ignored if the previous processing is unfinished yet. Therefore, make the
    // processing finalize explicitly here.
    await flushScrollStyle();

    gURLBar.focus();
    gURLBar.setURI(makeURI(data.secondURL));
    await flushScrollStyle();

    Assert.equal(gURLBar.selectionStart, data.expectedSelectionStart);
    Assert.equal(gURLBar.selectionEnd, data.expectedSelectionEnd);
    if (data.secondURL.length === data.expectedSelectionStart) {
      // If the caret is at the end of url, the input field shows the end of
      // text.
      Assert.equal(
        gURLBar.inputField.scrollLeft,
        gURLBar.inputField.scrollLeftMax
      );
    }

    info("Check the caret position while the input is not focused");
    gURLBar.setURI(makeURI(data.firstURL));
    gURLBar.selectionStart = data.initialSelectionStart;
    gURLBar.selectionEnd = data.initialSelectionEnd;

    await flushScrollStyle();

    gURLBar.blur();
    gURLBar.setURI(makeURI(data.secondURL));
    await flushScrollStyle();

    if (data.firstURL === data.secondURL) {
      Assert.equal(gURLBar.selectionStart, data.initialSelectionStart);
      Assert.equal(gURLBar.selectionEnd, data.initialSelectionEnd);
    } else {
      Assert.equal(gURLBar.selectionStart, gURLBar.value.length);
      Assert.equal(gURLBar.selectionEnd, gURLBar.value.length);
    }
    Assert.equal(gURLBar.inputField.scrollLeft, 0);
  }
});

// Tests that up and down keys move the caret on certain platforms, and that
// opening the popup doesn't change the caret position.
add_task(async function navigation() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "This is a generic sentence",
  });
  await UrlbarTestUtils.promisePopupClose(window);

  const INITIAL_SELECTION_START = 3;
  const INITIAL_SELECTION_END = 10;
  gURLBar.selectionStart = INITIAL_SELECTION_START;
  gURLBar.selectionEnd = INITIAL_SELECTION_END;

  if (AppConstants.platform == "macosx" || AppConstants.platform == "linux") {
    await checkCaretMoves(
      "KEY_ArrowDown",
      gURLBar.value.length,
      "Caret should have moved to the end",
      window
    );
    await checkPopupOpens("KEY_ArrowDown", window);

    await checkCaretMoves(
      "KEY_ArrowUp",
      0,
      "Caret should have moved to the start",
      window
    );
    await checkPopupOpens("KEY_ArrowUp", window);
  } else {
    await checkPopupOpens("KEY_ArrowDown", window);
    await checkPopupOpens("KEY_ArrowUp", window);
  }
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

async function flushScrollStyle() {
  // Flush pending notifications for the style.
  /* eslint-disable no-unused-expressions */
  gURLBar.inputField.scrollLeft;
  // Ensure to apply the style.
  await new Promise(resolve =>
    gURLBar.inputField.ownerGlobal.requestAnimationFrame(resolve)
  );
}
