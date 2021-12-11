/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that we show the inspect item(s) as appropriate.
 */
add_task(async function test_contextmenu_inspect() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["devtools.selfxss.count", 0],
      ["devtools.everOpened", false],
    ],
  });
  let contextMenu = document.getElementById("contentAreaContextMenu");
  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    for (let [pref, value, expectation] of [
      ["devtools.selfxss.count", 10, true],
      ["devtools.selfxss.count", 0, false],
      ["devtools.everOpened", false, false],
      ["devtools.everOpened", true, true],
    ]) {
      await SpecialPowers.pushPrefEnv({
        set: [["devtools.selfxss.count", value]],
      });
      is(contextMenu.state, "closed", "checking if popup is closed");
      let promisePopupShown = BrowserTestUtils.waitForEvent(
        contextMenu,
        "popupshown"
      );
      let promisePopupHidden = BrowserTestUtils.waitForEvent(
        contextMenu,
        "popuphidden"
      );
      await BrowserTestUtils.synthesizeMouse(
        "body",
        2,
        2,
        { type: "contextmenu", button: 2 },
        browser
      );
      await promisePopupShown;
      let inspectItem = document.getElementById("context-inspect");
      ok(
        !inspectItem.hidden,
        `Inspect should be shown (pref ${pref} is ${value}).`
      );
      let inspectA11y = document.getElementById("context-inspect-a11y");
      is(
        inspectA11y.hidden,
        !expectation,
        `A11y should be ${
          expectation ? "visible" : "hidden"
        } (pref ${pref} is ${value}).`
      );
      contextMenu.hidePopup();
      await promisePopupHidden;
    }
  });
});
