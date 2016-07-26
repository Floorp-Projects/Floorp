/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* exported CustomizableUI makeWidgetId focusWindow forceGC
 *          getBrowserActionWidget
 *          clickBrowserAction clickPageAction
 *          getBrowserActionPopup getPageActionPopup
 *          closeBrowserAction closePageAction
 *          promisePopupShown promisePopupHidden
 *          openContextMenu closeContextMenu
 *          openExtensionContextMenu closeExtensionContextMenu
 *          imageBuffer getListStyleImage
 */

var {AppConstants} = Cu.import("resource://gre/modules/AppConstants.jsm");
var {CustomizableUI} = Cu.import("resource:///modules/CustomizableUI.jsm");

// Bug 1239884: Our tests occasionally hit a long GC pause at unpredictable
// times in debug builds, which results in intermittent timeouts. Until we have
// a better solution, we force a GC after certain strategic tests, which tend to
// accumulate a high number of unreaped windows.
function forceGC() {
  if (AppConstants.DEBUG) {
    Cu.forceGC();
  }
}

function makeWidgetId(id) {
  id = id.toLowerCase();
  return id.replace(/[^a-z0-9_-]/g, "_");
}

var focusWindow = Task.async(function* focusWindow(win) {
  let fm = Cc["@mozilla.org/focus-manager;1"].getService(Ci.nsIFocusManager);
  if (fm.activeWindow == win) {
    return;
  }

  let promise = new Promise(resolve => {
    win.addEventListener("focus", function listener() {
      win.removeEventListener("focus", listener, true);
      resolve();
    }, true);
  });

  win.focus();
  yield promise;
});

let img = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQImWNgYGBgAAAABQABh6FO1AAAAABJRU5ErkJggg==";
var imageBuffer = Uint8Array.from(atob(img), byte => byte.charCodeAt(0)).buffer;

function getListStyleImage(button) {
  let style = button.ownerDocument.defaultView.getComputedStyle(button);

  let match = /^url\("(.*)"\)$/.exec(style.listStyleImage);

  return match && match[1];
}

function promisePopupShown(popup) {
  return new Promise(resolve => {
    if (popup.state == "open") {
      resolve();
    } else {
      let onPopupShown = event => {
        popup.removeEventListener("popupshown", onPopupShown);
        resolve();
      };
      popup.addEventListener("popupshown", onPopupShown);
    }
  });
}

function promisePopupHidden(popup) {
  return new Promise(resolve => {
    let onPopupHidden = event => {
      popup.removeEventListener("popuphidden", onPopupHidden);
      resolve();
    };
    popup.addEventListener("popuphidden", onPopupHidden);
  });
}

function getBrowserActionWidget(extension) {
  return CustomizableUI.getWidget(makeWidgetId(extension.id) + "-browser-action");
}

function getBrowserActionPopup(extension, win = window) {
  let group = getBrowserActionWidget(extension);

  if (group.areaType == CustomizableUI.TYPE_TOOLBAR) {
    return win.document.getElementById("customizationui-widget-panel");
  }
  return win.PanelUI.panel;
}

var showBrowserAction = Task.async(function* (extension, win = window) {
  let group = getBrowserActionWidget(extension);
  let widget = group.forWindow(win);

  if (group.areaType == CustomizableUI.TYPE_TOOLBAR) {
    ok(!widget.overflowed, "Expect widget not to be overflowed");
  } else if (group.areaType == CustomizableUI.TYPE_MENU_PANEL) {
    yield win.PanelUI.show();
  }
});

var clickBrowserAction = Task.async(function* (extension, win = window) {
  yield showBrowserAction(extension, win);

  let widget = getBrowserActionWidget(extension).forWindow(win);

  EventUtils.synthesizeMouseAtCenter(widget.node, {}, win);
});

function closeBrowserAction(extension, win = window) {
  let group = getBrowserActionWidget(extension);

  let node = win.document.getElementById(group.viewId);
  CustomizableUI.hidePanelForNode(node);

  return Promise.resolve();
}

function* openContextMenu(selector = "#img1") {
  let contentAreaContextMenu = document.getElementById("contentAreaContextMenu");
  let popupShownPromise = BrowserTestUtils.waitForEvent(contentAreaContextMenu, "popupshown");
  yield BrowserTestUtils.synthesizeMouseAtCenter(selector, {type: "contextmenu"}, gBrowser.selectedBrowser);
  yield popupShownPromise;
  return contentAreaContextMenu;
}

function* closeContextMenu() {
  let contentAreaContextMenu = document.getElementById("contentAreaContextMenu");
  let popupHiddenPromise = BrowserTestUtils.waitForEvent(contentAreaContextMenu, "popuphidden");
  contentAreaContextMenu.hidePopup();
  yield popupHiddenPromise;
}

function* openExtensionContextMenu(selector = "#img1") {
  let contextMenu = yield openContextMenu(selector);
  let topLevelMenu = contextMenu.getElementsByAttribute("ext-type", "top-level-menu");

  // Return null if the extension only has one item and therefore no extension menu.
  if (topLevelMenu.length == 0) {
    return null;
  }

  let extensionMenu = topLevelMenu[0].childNodes[0];
  let popupShownPromise = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(extensionMenu, {});
  yield popupShownPromise;
  return extensionMenu;
}

function* closeExtensionContextMenu(itemToSelect) {
  let contentAreaContextMenu = document.getElementById("contentAreaContextMenu");
  let popupHiddenPromise = BrowserTestUtils.waitForEvent(contentAreaContextMenu, "popuphidden");
  EventUtils.synthesizeMouseAtCenter(itemToSelect, {});
  yield popupHiddenPromise;
}

function getPageActionPopup(extension, win = window) {
  let panelId = makeWidgetId(extension.id) + "-panel";
  return win.document.getElementById(panelId);
}

function clickPageAction(extension, win = window) {
  // This would normally be set automatically on navigation, and cleared
  // when the user types a value into the URL bar, to show and hide page
  // identity info and icons such as page action buttons.
  //
  // Unfortunately, that doesn't happen automatically in browser chrome
  // tests.
  /* globals SetPageProxyState */
  SetPageProxyState("valid");

  let pageActionId = makeWidgetId(extension.id) + "-page-action";
  let elem = win.document.getElementById(pageActionId);

  EventUtils.synthesizeMouseAtCenter(elem, {}, win);
  return new Promise(SimpleTest.executeSoon);
}

function closePageAction(extension, win = window) {
  let node = getPageActionPopup(extension, win);
  if (node) {
    return promisePopupShown(node).then(() => {
      node.hidePopup();
    });
  }

  return Promise.resolve();
}
