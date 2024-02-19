/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

async function getCaretRect(browser, id) {
  // The caret rect can only be queried on LocalAccessible. On Windows, we do
  // send it across processes with caret events, but this currently can't be
  // queried outside of the event, nor with XPCOM.
  const [x, y, w, h] = await invokeContentTask(browser, [id], contentId => {
    const node = content.document.getElementById(contentId);
    const contentAcc = content.CommonUtils.accService.getAccessibleFor(node);
    contentAcc.QueryInterface(Ci.nsIAccessibleText);
    const caretX = {};
    const caretY = {};
    const caretW = {};
    const caretH = {};
    contentAcc.getCaretRect(caretX, caretY, caretW, caretH);
    return [caretX.value, caretY.value, caretW.value, caretH.value];
  });
  info(`Caret bounds: ${x}, ${y}, ${w}, ${h}`);
  return [x, y, w, h];
}

async function testCaretRect(browser, docAcc, id, offset) {
  const acc = findAccessibleChildByID(docAcc, id, [nsIAccessibleText]);
  is(acc.caretOffset, offset, `Caret at offset ${offset}`);
  const charX = {};
  const charY = {};
  const charW = {};
  const charH = {};
  const atEnd = offset == acc.characterCount;
  const empty = offset == 0 && atEnd;
  const queryOffset = atEnd && !empty ? offset - 1 : offset;
  acc.getCharacterExtents(
    queryOffset,
    charX,
    charY,
    charW,
    charH,
    COORDTYPE_SCREEN_RELATIVE
  );
  info(
    `Character ${queryOffset} bounds: ${charX.value}, ${charY.value}, ${charW.value}, ${charH.value}`
  );
  const [caretX, caretY, caretW, caretH] = await getCaretRect(browser, id);
  if (atEnd) {
    Assert.greater(caretX, charX.value, "Caret x after last character x");
  } else {
    is(caretX, charX.value, "Caret x same as character x");
  }
  is(caretY, charY.value, "Caret y same as character y");
  is(caretW, 1, "Caret width is 1");
  if (!empty) {
    is(caretH, charH.value, "Caret height same as character height");
  }
}

function getAccBounds(acc) {
  const x = {};
  const y = {};
  const w = {};
  const h = {};
  acc.getBounds(x, y, w, h);
  return [x.value, y.value, w.value, h.value];
}

/**
 * Test the caret rect in content documents.
 */
addAccessibleTask(
  `
<input id="input" value="ab">
<input id="emptyInput">
  `,
  async function (browser, docAcc) {
    async function runTests() {
      const input = findAccessibleChildByID(docAcc, "input", [
        nsIAccessibleText,
      ]);
      info("Focusing input");
      let caretMoved = waitForEvent(EVENT_TEXT_CARET_MOVED, input);
      input.takeFocus();
      await caretMoved;
      await testCaretRect(browser, docAcc, "input", 0);
      info("Setting caretOffset to 1");
      caretMoved = waitForEvent(EVENT_TEXT_CARET_MOVED, input);
      input.caretOffset = 1;
      await caretMoved;
      await testCaretRect(browser, docAcc, "input", 1);
      info("Setting caretOffset to 2");
      caretMoved = waitForEvent(EVENT_TEXT_CARET_MOVED, input);
      input.caretOffset = 2;
      await caretMoved;
      await testCaretRect(browser, docAcc, "input", 2);
      info("Resetting caretOffset to 0");
      input.caretOffset = 0;

      const emptyInput = findAccessibleChildByID(docAcc, "emptyInput", [
        nsIAccessibleText,
      ]);
      info("Focusing emptyInput");
      caretMoved = waitForEvent(EVENT_TEXT_CARET_MOVED, emptyInput);
      emptyInput.takeFocus();
      await caretMoved;
      await testCaretRect(browser, docAcc, "emptyInput", 0);
    }

    await runTests();

    // Check that the caret rect is correct when the title bar is shown.
    if (LINUX || Services.env.get("MOZ_HEADLESS")) {
      // Disabling tabs in title bar doesn't change the bounds on Linux or in
      // headless mode.
      info("Skipping title bar tests");
      return;
    }
    const [, origDocY] = getAccBounds(docAcc);
    info("Showing title bar");
    let titleBarChanged = BrowserTestUtils.waitForMutationCondition(
      document.documentElement,
      { attributes: true, attributeFilter: ["tabsintitlebar"] },
      () => !document.documentElement.hasAttribute("tabsintitlebar")
    );
    await SpecialPowers.pushPrefEnv({
      set: [["browser.tabs.inTitlebar", false]],
    });
    await titleBarChanged;
    const [, newDocY] = getAccBounds(docAcc);
    Assert.greater(
      newDocY,
      origDocY,
      "Doc has larger y after title bar change"
    );
    await runTests();
    await SpecialPowers.popPrefEnv();
  },
  { chrome: true, topLevel: true }
);
