/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let contextMenu;

const example_base =
  "http://example.com/browser/browser/base/content/test/contextMenu/";
const chrome_base =
  "chrome://mochitests/content/browser/browser/base/content/test/contextMenu/";

/* import-globals-from contextmenu_common.js */
Services.scriptloader.loadSubScript(
  chrome_base + "contextmenu_common.js",
  this
);

async function openMenuAndPaste(browser, useFormatting) {
  const kElementToUse = "test-contenteditable-spellcheck-false";
  let oldText = await SpecialPowers.spawn(browser, [kElementToUse], elemID => {
    return content.document.getElementById(elemID).textContent;
  });

  // Open context menu and paste
  await test_contextmenu(
    "#" + kElementToUse,
    [
      "context-undo",
      null, // whether we can undo changes mid-test.
      "context-redo",
      false,
      "---",
      null,
      "context-cut",
      false,
      "context-copy",
      false,
      "context-paste",
      true,
      "context-paste-no-formatting",
      true,
      "context-delete",
      false,
      "context-selectall",
      true,
    ],
    {
      keepMenuOpen: true,
    }
  );
  let popupHidden = BrowserTestUtils.waitForPopupEvent(contextMenu, "hidden");
  let menuID = "context-paste" + (useFormatting ? "" : "-no-formatting");
  contextMenu.activateItem(document.getElementById(menuID));
  await popupHidden;
  await SpecialPowers.spawn(
    browser,
    [kElementToUse, oldText, useFormatting],
    (elemID, textToReset, expectStrong) => {
      let node = content.document.getElementById(elemID);
      Assert.stringContains(
        node.textContent,
        "Bold text",
        "Text should have been pasted"
      );
      if (expectStrong) {
        isnot(
          node.querySelector("strong"),
          null,
          "Should be markup in the text."
        );
      } else {
        is(
          node.querySelector("strong"),
          null,
          "Should be no markup in the text."
        );
      }
      node.textContent = textToReset;
    }
  );
}

add_task(async function test_contenteditable() {
  // Put some HTML on the clipboard:
  const xferable = Cc["@mozilla.org/widget/transferable;1"].createInstance(
    Ci.nsITransferable
  );
  xferable.init(null);
  xferable.addDataFlavor("text/html");
  xferable.setTransferData(
    "text/html",
    PlacesUtils.toISupportsString("<strong>Bold text</strong>")
  );
  xferable.addDataFlavor("text/unicode");
  xferable.setTransferData(
    "text/unicode",
    PlacesUtils.toISupportsString("Bold text")
  );
  Services.clipboard.setData(
    xferable,
    null,
    Services.clipboard.kGlobalClipboard
  );

  let url = example_base + "subtst_contextmenu.html";
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
    },
    async function(browser) {
      await openMenuAndPaste(browser, false);
      await openMenuAndPaste(browser, true);
    }
  );
});
