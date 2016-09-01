/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for Bug 1297539
 * Test that the content event "pasteTransferable"
 * (mozilla::EventMessage::eContentCommandPasteTransferable)
 * is handled correctly for plain text and html in the remote case.
 *
 * Original test test_bug525389.html for command content event
 * "pasteTransferable" runs only in the content process.
 * This doesn't test the remote case.
 *
 */

"use strict";

function getLoadContext() {
  return window.QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIWebNavigation)
               .QueryInterface(Ci.nsILoadContext);
}

function getTransferableFromClipboard(asHTML) {
  let trans = Cc["@mozilla.org/widget/transferable;1"].
                    createInstance(Ci.nsITransferable);
  trans.init(getLoadContext());
  if (asHTML) {
    trans.addDataFlavor("text/html");
  } else {
    trans.addDataFlavor("text/unicode");
  }
  let clip = Cc["@mozilla.org/widget/clipboard;1"].getService(Ci.nsIClipboard);
  clip.getData(trans, Ci.nsIClipboard.kGlobalClipboard);
  return trans;
}

function* cutCurrentSelection(elementQueryString, property, browser) {
  // Cut the current selection.
  yield BrowserTestUtils.synthesizeKey("x", {accelKey: true}, browser);

  // The editor should be empty after cut.
  yield ContentTask.spawn(browser, {elementQueryString, property},
    function* ({elementQueryString, property}) {
      let element = content.document.querySelector(elementQueryString);
      is(element[property], "",
        `${elementQueryString} should be empty after cut (superkey + x)`);
    });
}

// Test that you are able to pasteTransferable for plain text
// which is handled by TextEditor::PasteTransferable to paste into the editor.
add_task(function* test_paste_transferable_plain_text()
{
  let testPage =
    'data:text/html,' +
    '<textarea id="textarea">Write something here</textarea>';

  yield BrowserTestUtils.withNewTab(testPage, function* (browser) {
    // Select all the content in your editor element.
    yield BrowserTestUtils.synthesizeMouse("#textarea", 0, 0, {}, browser);
    yield BrowserTestUtils.synthesizeKey("a", {accelKey: true}, browser);

    yield* cutCurrentSelection("#textarea", "value", browser);

    let trans = getTransferableFromClipboard(false);
    let DOMWindowUtils = EventUtils._getDOMWindowUtils(window);
    DOMWindowUtils.sendContentCommandEvent("pasteTransferable", trans);

    yield ContentTask.spawn(browser, null, function* () {
      let textArea = content.document.querySelector('#textarea');
      is(textArea.value, "Write something here",
         "Send content command pasteTransferable successful");
    });
  });
});

// Test that you are able to pasteTransferable for html
// which is handled by HTMLEditor::PasteTransferable to paste into the editor.
//
// On Linux,
// BrowserTestUtils.synthesizeKey("a", {accelKey: true}, browser);
// doesn't seem to trigger for contenteditable which is why we use
// Selection to select the contenteditable contents.
add_task(function* test_paste_transferable_html()
{
  let testPage =
    'data:text/html,' +
    '<div contenteditable="true"><b>Bold Text</b><i>italics</i></div>';

  yield BrowserTestUtils.withNewTab(testPage, function* (browser) {
    // Select all the content in your editor element.
    yield BrowserTestUtils.synthesizeMouse("div", 0, 0, {}, browser);
    yield ContentTask.spawn(browser, {}, function* () {
      let element = content.document.querySelector("div");
      let selection = content.window.getSelection();
      selection.selectAllChildren(element);
    });

    yield* cutCurrentSelection("div", "textContent", browser);

    let trans = getTransferableFromClipboard(true);
    let DOMWindowUtils = EventUtils._getDOMWindowUtils(window);
    DOMWindowUtils.sendContentCommandEvent("pasteTransferable", trans);

    yield ContentTask.spawn(browser, null, function* () {
      let textArea = content.document.querySelector('div');
      is(textArea.innerHTML, "<b>Bold Text</b><i>italics</i>",
         "Send content command pasteTransferable successful");
    });
  });
});
