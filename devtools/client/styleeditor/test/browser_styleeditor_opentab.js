/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// A test to check the 'Open Link in new tab' functionality in the
// context menu item for stylesheets (bug 992947).
const TESTCASE_URI = TEST_BASE_HTTPS + "simple.html";

add_task(async function() {
  const { ui } = await openStyleEditorForURL(TESTCASE_URI);

  await rightClickStyleSheet(ui, ui.editors[0]);
  is(ui._openLinkNewTabItem.getAttribute("disabled"), "false",
    "The menu item is not disabled");
  is(ui._openLinkNewTabItem.getAttribute("hidden"), "false",
    "The menu item is not hidden");

  const url = "https://example.com/browser/devtools/client/styleeditor/test/" +
    "simple.css";
  is(ui._contextMenuStyleSheet.href, url, "Correct URL for sheet");

  const browserWindow = Services.wm.getMostRecentWindow(gDevTools.chromeWindowType);
  const originalOpenWebLinkIn = browserWindow.openWebLinkIn;
  const tabOpenedDefer = new Promise(resolve => {
    browserWindow.openWebLinkIn = newUrl => {
      // Reset the actual openWebLinkIn function before proceeding.
      browserWindow.openWebLinkIn = originalOpenWebLinkIn;

      is(newUrl, url, "The correct tab has been opened");
      resolve();
    };
  });

  ui._openLinkNewTabItem.click();

  info(`Waiting for a tab to open - ${url}`);
  await tabOpenedDefer;

  await rightClickInlineStyleSheet(ui, ui.editors[1]);
  is(ui._openLinkNewTabItem.getAttribute("disabled"), "true",
    "The menu item is disabled");
  is(ui._openLinkNewTabItem.getAttribute("hidden"), "false",
    "The menu item is not hidden");

  await rightClickNoStyleSheet(ui);
  is(ui._openLinkNewTabItem.getAttribute("hidden"), "true",
    "The menu item is not hidden");
});

function onPopupShow(contextMenu) {
  return new Promise(resolve => {
    contextMenu.addEventListener("popupshown", function() {
      resolve();
    }, {once: true});
  });
}

function onPopupHide(contextMenu) {
  return new Promise(resolve => {
    contextMenu.addEventListener("popuphidden", function() {
      resolve();
    }, {once: true});
  });
}

function rightClickStyleSheet(ui, editor) {
  return new Promise(resolve => {
    onPopupShow(ui._contextMenu).then(()=> {
      onPopupHide(ui._contextMenu).then(() => {
        resolve();
      });
      ui._contextMenu.hidePopup();
    });

    EventUtils.synthesizeMouseAtCenter(
      editor.summary.querySelector(".stylesheet-name"),
      {button: 2, type: "contextmenu"},
      ui._window);
  });
}

function rightClickInlineStyleSheet(ui, editor) {
  return new Promise(resolve => {
    onPopupShow(ui._contextMenu).then(()=> {
      onPopupHide(ui._contextMenu).then(() => {
        resolve();
      });
      ui._contextMenu.hidePopup();
    });

    EventUtils.synthesizeMouseAtCenter(
      editor.summary.querySelector(".stylesheet-name"),
      {button: 2, type: "contextmenu"},
      ui._window);
  });
}

function rightClickNoStyleSheet(ui) {
  return new Promise(resolve => {
    onPopupShow(ui._contextMenu).then(()=> {
      onPopupHide(ui._contextMenu).then(() => {
        resolve();
      });
      ui._contextMenu.hidePopup();
    });

    EventUtils.synthesizeMouseAtCenter(
      ui._panelDoc.querySelector("#splitview-tpl-summary-stylesheet"),
      {button: 2, type: "contextmenu"},
      ui._window);
  });
}
