/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

add_task(async function() {
  const url =
    "http://mochi.test:8888/browser/toolkit/content/tests/browser/doggy.png";

  await SpecialPowers.pushPrefEnv({
    set: [["dom.text-recognition.enabled", true]],
  });

  await BrowserTestUtils.withNewTab(url, async function(browser) {
    setClipboardText("");
    is(getTextFromClipboard(), "", "The copied text is empty.");

    info("Right click image to show context menu.");
    let popupShownPromise = BrowserTestUtils.waitForEvent(
      document,
      "popupshown"
    );
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "img",
      { type: "contextmenu", button: 2 },
      browser
    );
    await popupShownPromise;

    info("Click context menu to copy the image text.");
    document.getElementById("context-imagetext").doCommand();

    info("Close the context menu.");
    let contextMenu = document.getElementById("contentAreaContextMenu");
    let popupHiddenPromise = BrowserTestUtils.waitForEvent(
      contextMenu,
      "popuphidden"
    );
    contextMenu.hidePopup();
    await popupHiddenPromise;

    info("Waiting for the dialog browser to be shown.");
    const { contentDocument } = await BrowserTestUtils.waitForCondition(() =>
      document.querySelector(".textRecognitionDialogFrame")
    );

    info("Waiting for no results message.");
    const noResultsHeader = contentDocument.querySelector(
      "#text-recognition-header-no-results"
    );
    await BrowserTestUtils.waitForCondition(() => {
      return noResultsHeader.style.display !== "none";
    });

    const text = contentDocument.querySelector(".textRecognitionText");
    is(text.children.length, 0, "No results are listed.");

    info("Close the dialog box.");
    const close = contentDocument.querySelector("#text-recognition-close");
    close.click();

    info("Waiting for the dialog frame to close.");
    await BrowserTestUtils.waitForCondition(
      () => !document.querySelector(".textRecognitionDialogFrame")
    );

    is(getTextFromClipboard(), "", "The copied text is still empty.");
  });
});
