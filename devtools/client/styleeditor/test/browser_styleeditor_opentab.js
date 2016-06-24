/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// A test to check the 'Open Link in new tab' functionality in the
// context menu item for stylesheets (bug 992947).
const TESTCASE_URI = TEST_BASE_HTTPS + "simple.html";

add_task(function* () {
  let { ui } = yield openStyleEditorForURL(TESTCASE_URI);

  yield rightClickStyleSheet(ui, ui.editors[0]);
  is(ui._openLinkNewTabItem.getAttribute("disabled"), "false",
    "The menu item is not disabled");
  is(ui._openLinkNewTabItem.getAttribute("hidden"), "false",
    "The menu item is not hidden");

  let url = "https://example.com/browser/devtools/client/styleeditor/test/" +
    "simple.css";
  is(ui._contextMenuStyleSheet.href, url, "Correct URL for sheet");

  let originalOpenUILinkIn = ui._window.openUILinkIn;
  let tabOpenedDefer = defer();

  ui._window.openUILinkIn = newUrl => {
    // Reset the actual openUILinkIn function before proceeding.
    ui._window.openUILinkIn = originalOpenUILinkIn;

    is(newUrl, url, "The correct tab has been opened");
    tabOpenedDefer.resolve();
  };

  ui._openLinkNewTabItem.click();

  info(`Waiting for a tab to open - ${url}`);
  yield tabOpenedDefer.promise;

  yield rightClickInlineStyleSheet(ui, ui.editors[1]);
  is(ui._openLinkNewTabItem.getAttribute("disabled"), "true",
    "The menu item is disabled");
  is(ui._openLinkNewTabItem.getAttribute("hidden"), "false",
    "The menu item is not hidden");

  yield rightClickNoStyleSheet(ui);
  is(ui._openLinkNewTabItem.getAttribute("hidden"), "true",
    "The menu item is not hidden");
});

function onPopupShow(contextMenu) {
  let deferred = defer();
  contextMenu.addEventListener("popupshown", function onpopupshown() {
    contextMenu.removeEventListener("popupshown", onpopupshown);
    deferred.resolve();
  });
  return deferred.promise;
}

function onPopupHide(contextMenu) {
  let deferred = defer();
  contextMenu.addEventListener("popuphidden", function popuphidden() {
    contextMenu.removeEventListener("popuphidden", popuphidden);
    deferred.resolve();
  });
  return deferred.promise;
}

function rightClickStyleSheet(ui, editor) {
  let deferred = defer();

  onPopupShow(ui._contextMenu).then(()=> {
    onPopupHide(ui._contextMenu).then(() => {
      deferred.resolve();
    });
    ui._contextMenu.hidePopup();
  });

  EventUtils.synthesizeMouseAtCenter(
    editor.summary.querySelector(".stylesheet-name"),
    {button: 2, type: "contextmenu"},
    ui._window);

  return deferred.promise;
}

function rightClickInlineStyleSheet(ui, editor) {
  let deferred = defer();

  onPopupShow(ui._contextMenu).then(()=> {
    onPopupHide(ui._contextMenu).then(() => {
      deferred.resolve();
    });
    ui._contextMenu.hidePopup();
  });

  EventUtils.synthesizeMouseAtCenter(
    editor.summary.querySelector(".stylesheet-name"),
    {button: 2, type: "contextmenu"},
    ui._window);

  return deferred.promise;
}

function rightClickNoStyleSheet(ui) {
  let deferred = defer();

  onPopupShow(ui._contextMenu).then(()=> {
    onPopupHide(ui._contextMenu).then(() => {
      deferred.resolve();
    });
    ui._contextMenu.hidePopup();
  });

  EventUtils.synthesizeMouseAtCenter(
    ui._panelDoc.querySelector("#splitview-tpl-summary-stylesheet"),
    {button: 2, type: "contextmenu"},
    ui._window);

  return deferred.promise;
}
