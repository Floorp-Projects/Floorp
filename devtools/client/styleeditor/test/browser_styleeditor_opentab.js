/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// A test to check the 'Open Link in new tab' functionality in the
// context menu item for stylesheets (bug 992947).
const TESTCASE_URI = TEST_BASE_HTTPS + "simple.html";

add_task(async function () {
  const { panel, ui } = await openStyleEditorForURL(TESTCASE_URI);

  const openLinkNewTabItem = panel.panelWindow.document.getElementById(
    "context-openlinknewtab"
  );

  let menu = await rightClickStyleSheet(panel, ui.editors[0]);
  is(
    openLinkNewTabItem.getAttribute("disabled"),
    "false",
    "The menu item is not disabled"
  );
  ok(!openLinkNewTabItem.hidden, "The menu item is not hidden");

  const url = TEST_BASE_HTTPS + "simple.css";

  const browserWindow = Services.wm.getMostRecentWindow(
    gDevTools.chromeWindowType
  );
  const originalOpenWebLinkIn = browserWindow.openWebLinkIn;
  const tabOpenedDefer = new Promise(resolve => {
    browserWindow.openWebLinkIn = newUrl => {
      // Reset the actual openWebLinkIn function before proceeding.
      browserWindow.openWebLinkIn = originalOpenWebLinkIn;

      is(newUrl, url, "The correct tab has been opened");
      resolve();
    };
  });

  const hidden = onPopupHide(menu);

  menu.activateItem(openLinkNewTabItem);

  info(`Waiting for a tab to open - ${url}`);
  await tabOpenedDefer;

  await hidden;

  menu = await rightClickInlineStyleSheet(panel, ui.editors[1]);
  is(
    openLinkNewTabItem.getAttribute("disabled"),
    "true",
    "The menu item is disabled"
  );
  ok(!openLinkNewTabItem.hidden, "The menu item should not be hidden");
  menu.hidePopup();

  menu = await rightClickNoStyleSheet(panel);
  ok(openLinkNewTabItem.hidden, "The menu item should be hidden");
  menu.hidePopup();
});

function onPopupShow(contextMenu) {
  return new Promise(resolve => {
    contextMenu.addEventListener(
      "popupshown",
      function () {
        resolve();
      },
      { once: true }
    );
  });
}

function onPopupHide(contextMenu) {
  return new Promise(resolve => {
    contextMenu.addEventListener(
      "popuphidden",
      function () {
        resolve();
      },
      { once: true }
    );
  });
}

function rightClickStyleSheet(panel, editor) {
  const contextMenu = getContextMenuElement(panel);
  return new Promise(resolve => {
    onPopupShow(contextMenu).then(() => {
      resolve(contextMenu);
    });

    EventUtils.synthesizeMouseAtCenter(
      editor.summary.querySelector(".stylesheet-name"),
      { button: 2, type: "contextmenu" },
      panel.panelWindow
    );
  });
}

function rightClickInlineStyleSheet(panel, editor) {
  const contextMenu = getContextMenuElement(panel);
  return new Promise(resolve => {
    onPopupShow(contextMenu).then(() => {
      resolve(contextMenu);
    });

    EventUtils.synthesizeMouseAtCenter(
      editor.summary.querySelector(".stylesheet-name"),
      { button: 2, type: "contextmenu" },
      panel.panelWindow
    );
  });
}

function rightClickNoStyleSheet(panel) {
  const contextMenu = getContextMenuElement(panel);
  return new Promise(resolve => {
    onPopupShow(contextMenu).then(() => {
      resolve(contextMenu);
    });

    EventUtils.synthesizeMouseAtCenter(
      panel.panelWindow.document.querySelector(
        "#splitview-tpl-summary-stylesheet"
      ),
      { button: 2, type: "contextmenu" },
      panel.panelWindow
    );
  });
}
