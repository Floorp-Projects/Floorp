/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* exported CustomizableUI makeWidgetId focusWindow forceGC
 *          getBrowserActionWidget
 *          clickBrowserAction clickPageAction clickPageActionInPanel
 *          triggerPageActionWithKeyboard triggerPageActionWithKeyboardInPanel
 *          triggerBrowserActionWithKeyboard
 *          getBrowserActionPopup getPageActionPopup getPageActionButton
 *          openBrowserActionPanel
 *          closeBrowserAction closePageAction
 *          promisePopupShown promisePopupHidden promisePopupNotificationShown
 *          toggleBookmarksToolbar
 *          openContextMenu closeContextMenu
 *          openContextMenuInSidebar openContextMenuInPopup
 *          openExtensionContextMenu closeExtensionContextMenu
 *          openActionContextMenu openSubmenu closeActionContextMenu
 *          openTabContextMenu closeTabContextMenu
 *          openToolsMenu closeToolsMenu
 *          imageBuffer imageBufferFromDataURI
 *          getInlineOptionsBrowser
 *          getListStyleImage getPanelForNode
 *          awaitExtensionPanel awaitPopupResize
 *          promiseContentDimensions alterContent
 *          promisePrefChangeObserved openContextMenuInFrame
 *          promiseAnimationFrame getCustomizableUIPanelID
 *          awaitEvent BrowserWindowIterator
 *          navigateTab historyPushState promiseWindowRestored
 *          getIncognitoWindow startIncognitoMonitorExtension
 *          loadTestSubscript awaitBrowserLoaded backgroundColorSetOnRoot
 */

// There are shutdown issues for which multiple rejections are left uncaught.
// This bug should be fixed, but for the moment all tests in this directory
// allow various classes of promise rejections.
//
// NOTE: Allowing rejections on an entire directory should be avoided.
//       Normally you should use "expectUncaughtRejection" to flag individual
//       failures.
const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);
PromiseTestUtils.allowMatchingRejectionsGlobally(
  /Message manager disconnected/
);
PromiseTestUtils.allowMatchingRejectionsGlobally(/No matching message handler/);
PromiseTestUtils.allowMatchingRejectionsGlobally(
  /Receiving end does not exist/
);

const { AppUiTestDelegate, AppUiTestInternals } = ChromeUtils.import(
  "resource://testing-common/AppUiTestDelegate.jsm"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { CustomizableUI } = ChromeUtils.import(
  "resource:///modules/CustomizableUI.jsm"
);
const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);
const { ClientEnvironmentBase } = ChromeUtils.import(
  "resource://gre/modules/components-utils/ClientEnvironment.jsm"
);

XPCOMUtils.defineLazyGetter(this, "Management", () => {
  const { Management } = ChromeUtils.import(
    "resource://gre/modules/Extension.jsm",
    null
  );
  return Management;
});

var {
  makeWidgetId,
  promisePopupShown,
  getPanelForNode,
  awaitBrowserLoaded,
} = AppUiTestInternals;

// The extension tests can run a lot slower under ASAN.
if (AppConstants.ASAN) {
  SimpleTest.requestLongerTimeout(10);
}

function loadTestSubscript(filePath) {
  Services.scriptloader.loadSubScript(new URL(filePath, gTestPath).href, this);
}

// Leaving Top Sites enabled during these tests would create site screenshots
// and update pinned Top Sites unnecessarily.
Services.prefs
  .getDefaultBranch("browser.newtabpage.activity-stream.")
  .setBoolPref("feeds.topsites", false);
Services.prefs
  .getDefaultBranch("browser.newtabpage.activity-stream.")
  .setBoolPref("feeds.system.topsites", false);

{
  // Touch the recipeParentPromise lazy getter so we don't get
  // `this._recipeManager is undefined` errors during tests.
  const { LoginManagerParent } = ChromeUtils.import(
    "resource://gre/modules/LoginManagerParent.jsm"
  );
  void LoginManagerParent.recipeParentPromise;
}

// Bug 1239884: Our tests occasionally hit a long GC pause at unpredictable
// times in debug builds, which results in intermittent timeouts. Until we have
// a better solution, we force a GC after certain strategic tests, which tend to
// accumulate a high number of unreaped windows.
function forceGC() {
  if (AppConstants.DEBUG) {
    Cu.forceGC();
  }
}

var focusWindow = async function focusWindow(win) {
  if (Services.focus.activeWindow == win) {
    return;
  }

  let promise = new Promise(resolve => {
    win.addEventListener(
      "focus",
      function() {
        resolve();
      },
      { capture: true, once: true }
    );
  });

  win.focus();
  await promise;
};

function imageBufferFromDataURI(encodedImageData) {
  let decodedImageData = atob(encodedImageData);
  return Uint8Array.from(decodedImageData, byte => byte.charCodeAt(0)).buffer;
}

let img =
  "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQImWNgYGBgAAAABQABh6FO1AAAAABJRU5ErkJggg==";
var imageBuffer = imageBufferFromDataURI(img);

function getInlineOptionsBrowser(aboutAddonsBrowser) {
  let { contentDocument } = aboutAddonsBrowser;
  return contentDocument.getElementById("addon-inline-options");
}

function getListStyleImage(button) {
  let style = button.ownerGlobal.getComputedStyle(button);

  let match = /^url\("(.*)"\)$/.exec(style.listStyleImage);

  return match && match[1];
}

function promiseAnimationFrame(win = window) {
  return AppUiTestInternals.promiseAnimationFrame(win);
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

/**
 * Wait for the given PopupNotification to display
 *
 * @param {string} name
 *        The name of the notification to wait for.
 * @param {Window} [win]
 *        The chrome window in which to wait for the notification.
 *
 * @returns {Promise}
 *          Resolves with the notification window.
 */
function promisePopupNotificationShown(name, win = window) {
  return new Promise(resolve => {
    function popupshown() {
      let notification = win.PopupNotifications.getNotification(name);
      if (!notification) {
        return;
      }

      ok(notification, `${name} notification shown`);
      ok(win.PopupNotifications.isPanelOpen, "notification panel open");

      win.PopupNotifications.panel.removeEventListener(
        "popupshown",
        popupshown
      );
      resolve(win.PopupNotifications.panel.firstElementChild);
    }

    win.PopupNotifications.panel.addEventListener("popupshown", popupshown);
  });
}

function promisePossiblyInaccurateContentDimensions(browser) {
  return SpecialPowers.spawn(browser, [], async function() {
    function copyProps(obj, props) {
      let res = {};
      for (let prop of props) {
        res[prop] = obj[prop];
      }
      return res;
    }

    return {
      window: copyProps(content, [
        "innerWidth",
        "innerHeight",
        "outerWidth",
        "outerHeight",
        "scrollX",
        "scrollY",
        "scrollMaxX",
        "scrollMaxY",
      ]),
      body: copyProps(content.document.body, [
        "clientWidth",
        "clientHeight",
        "scrollWidth",
        "scrollHeight",
      ]),
      root: copyProps(content.document.documentElement, [
        "clientWidth",
        "clientHeight",
        "scrollWidth",
        "scrollHeight",
      ]),
      isStandards: content.document.compatMode !== "BackCompat",
    };
  });
}

function delay(ms = 0) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

async function promiseContentDimensions(browser) {
  // For remote browsers, each resize operation requires an asynchronous
  // round-trip to resize the content window. Since there's a certain amount of
  // unpredictability in the timing, mainly due to the unpredictability of
  // reflows, we need to wait until the content window dimensions match the
  // <browser> dimensions before returning data.

  let dims = await promisePossiblyInaccurateContentDimensions(browser);
  while (
    browser.clientWidth !== Math.round(dims.window.innerWidth) ||
    browser.clientHeight !== Math.round(dims.window.innerHeight)
  ) {
    await delay(50);
    dims = await promisePossiblyInaccurateContentDimensions(browser);
  }

  return dims;
}

async function awaitPopupResize(browser) {
  await BrowserTestUtils.waitForEvent(
    browser,
    "WebExtPopupResized",
    event => event.detail === "delayed"
  );

  return promiseContentDimensions(browser);
}

function alterContent(browser, task, arg = null) {
  return Promise.all([
    SpecialPowers.spawn(browser, [arg], task),
    awaitPopupResize(browser),
  ]).then(([, dims]) => dims);
}

async function focusButtonAndPressKey(key, elem, modifiers) {
  let focused = BrowserTestUtils.waitForEvent(elem, "focus", true);

  elem.setAttribute("tabindex", "-1");
  elem.focus();
  elem.removeAttribute("tabindex");
  await focused;

  EventUtils.synthesizeKey(key, modifiers);
  elem.blur();
}

var awaitExtensionPanel = function(extension, win = window, awaitLoad = true) {
  return AppUiTestDelegate.awaitExtensionPanel(win, extension.id, awaitLoad);
};

function getCustomizableUIPanelID() {
  return CustomizableUI.AREA_FIXED_OVERFLOW_PANEL;
}

function getBrowserActionWidget(extension) {
  return AppUiTestInternals.getBrowserActionWidget(extension.id);
}

function getBrowserActionPopup(extension, win = window) {
  let group = getBrowserActionWidget(extension);

  if (group.areaType == CustomizableUI.TYPE_TOOLBAR) {
    return win.document.getElementById("customizationui-widget-panel");
  }
  return win.PanelUI.overflowPanel;
}

var showBrowserAction = function(extension, win = window) {
  return AppUiTestInternals.showBrowserAction(win, extension.id);
};

function clickBrowserAction(extension, win = window, modifiers) {
  return AppUiTestDelegate.clickBrowserAction(win, extension.id, modifiers);
}

async function triggerBrowserActionWithKeyboard(
  extension,
  key = "KEY_Enter",
  modifiers = {},
  win = window
) {
  await promiseAnimationFrame(win);
  await showBrowserAction(extension, win);

  let group = getBrowserActionWidget(extension);
  let node = group.forWindow(win).node;

  if (group.areaType == CustomizableUI.TYPE_TOOLBAR) {
    await focusButtonAndPressKey(key, node, modifiers);
  } else if (group.areaType == CustomizableUI.TYPE_MENU_PANEL) {
    // Use key navigation so that the PanelMultiView doesn't ignore key events
    let panel = win.document.getElementById("widget-overflow");
    while (win.document.activeElement != node) {
      EventUtils.synthesizeKey("KEY_ArrowDown");
      ok(
        panel.contains(win.document.activeElement),
        "Focus is inside the panel"
      );
    }
    EventUtils.synthesizeKey(key, modifiers);
  }
}

function closeBrowserAction(extension, win = window) {
  return AppUiTestDelegate.closeBrowserAction(win, extension.id);
}

function openBrowserActionPanel(extension, win = window, awaitLoad = false) {
  clickBrowserAction(extension, win);

  return awaitExtensionPanel(extension, win, awaitLoad);
}

async function toggleBookmarksToolbar(visible = true) {
  let bookmarksToolbar = document.getElementById("PersonalToolbar");
  // Third parameter is 'persist' and true is the default.
  // Fourth parameter is 'animated' and we want no animation.
  setToolbarVisibility(bookmarksToolbar, visible, true, false);
  if (!visible) {
    return BrowserTestUtils.waitForMutationCondition(
      bookmarksToolbar,
      { attributes: true },
      () => bookmarksToolbar.collapsed
    );
  }

  return BrowserTestUtils.waitForEvent(
    bookmarksToolbar,
    "BookmarksToolbarVisibilityUpdated"
  );
}

async function openContextMenuInPopup(extension, selector = "body") {
  let contentAreaContextMenu = document.getElementById(
    "contentAreaContextMenu"
  );
  let browser = await awaitExtensionPanel(extension);

  // Ensure that the document layout has been flushed before triggering the mouse event
  // (See Bug 1519808 for a rationale).
  await browser.ownerGlobal.promiseDocumentFlushed(() => {});
  let popupShownPromise = BrowserTestUtils.waitForEvent(
    contentAreaContextMenu,
    "popupshown"
  );
  await BrowserTestUtils.synthesizeMouseAtCenter(
    selector,
    { type: "mousedown", button: 2 },
    browser
  );
  await BrowserTestUtils.synthesizeMouseAtCenter(
    selector,
    { type: "contextmenu" },
    browser
  );
  await popupShownPromise;
  return contentAreaContextMenu;
}

async function openContextMenuInSidebar(selector = "body") {
  let contentAreaContextMenu = SidebarUI.browser.contentDocument.getElementById(
    "contentAreaContextMenu"
  );
  let browser = SidebarUI.browser.contentDocument.getElementById(
    "webext-panels-browser"
  );
  let popupShownPromise = BrowserTestUtils.waitForEvent(
    contentAreaContextMenu,
    "popupshown"
  );
  await BrowserTestUtils.synthesizeMouseAtCenter(
    selector,
    { type: "mousedown", button: 2 },
    browser
  );
  await BrowserTestUtils.synthesizeMouseAtCenter(
    selector,
    { type: "contextmenu" },
    browser
  );
  await popupShownPromise;
  return contentAreaContextMenu;
}

// `selector` should refer to the content in the frame. If invalid the test can
// fail intermittently because the click could inadvertently be registered on
// the upper-left corner of the frame (instead of inside the frame).
async function openContextMenuInFrame(selector = "body", frameIndex = 0) {
  let contentAreaContextMenu = document.getElementById(
    "contentAreaContextMenu"
  );
  let popupShownPromise = BrowserTestUtils.waitForEvent(
    contentAreaContextMenu,
    "popupshown"
  );
  await BrowserTestUtils.synthesizeMouseAtCenter(
    selector,
    { type: "contextmenu" },
    gBrowser.selectedBrowser.browsingContext.children[frameIndex]
  );
  await popupShownPromise;
  return contentAreaContextMenu;
}

async function openContextMenu(selector = "#img1", win = window) {
  let contentAreaContextMenu = win.document.getElementById(
    "contentAreaContextMenu"
  );
  let popupShownPromise = BrowserTestUtils.waitForEvent(
    contentAreaContextMenu,
    "popupshown"
  );
  await BrowserTestUtils.synthesizeMouseAtCenter(
    selector,
    { type: "mousedown", button: 2 },
    win.gBrowser.selectedBrowser
  );
  await BrowserTestUtils.synthesizeMouseAtCenter(
    selector,
    { type: "contextmenu" },
    win.gBrowser.selectedBrowser
  );
  await popupShownPromise;
  return contentAreaContextMenu;
}

async function closeContextMenu(contextMenu) {
  let contentAreaContextMenu =
    contextMenu || document.getElementById("contentAreaContextMenu");
  let popupHiddenPromise = BrowserTestUtils.waitForEvent(
    contentAreaContextMenu,
    "popuphidden"
  );
  contentAreaContextMenu.hidePopup();
  await popupHiddenPromise;
}

async function openExtensionContextMenu(selector = "#img1") {
  let contextMenu = await openContextMenu(selector);
  let topLevelMenu = contextMenu.getElementsByAttribute(
    "ext-type",
    "top-level-menu"
  );

  // Return null if the extension only has one item and therefore no extension menu.
  if (!topLevelMenu.length) {
    return null;
  }

  let extensionMenu = topLevelMenu[0];
  let popupShownPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );
  extensionMenu.openMenu(true);
  await popupShownPromise;
  return extensionMenu;
}

async function closeExtensionContextMenu(itemToSelect, modifiers = {}) {
  let contentAreaContextMenu = document.getElementById(
    "contentAreaContextMenu"
  );
  let popupHiddenPromise = BrowserTestUtils.waitForEvent(
    contentAreaContextMenu,
    "popuphidden"
  );
  if (itemToSelect) {
    itemToSelect.closest("menupopup").activateItem(itemToSelect, modifiers);
  } else {
    contentAreaContextMenu.hidePopup();
  }
  await popupHiddenPromise;

  // Bug 1351638: parent menu fails to close intermittently, make sure it does.
  contentAreaContextMenu.hidePopup();
}

async function openToolsMenu(win = window) {
  const node = win.document.getElementById("tools-menu");
  const menu = win.document.getElementById("menu_ToolsPopup");
  const shown = BrowserTestUtils.waitForEvent(menu, "popupshown");
  if (AppConstants.platform === "macosx") {
    // We can't open menubar items on OSX, so mocking instead.
    menu.dispatchEvent(new MouseEvent("popupshowing"));
    menu.dispatchEvent(new MouseEvent("popupshown"));
  } else {
    node.open = true;
  }
  await shown;
  return menu;
}

function closeToolsMenu(itemToSelect, win = window) {
  const menu = win.document.getElementById("menu_ToolsPopup");
  const hidden = BrowserTestUtils.waitForEvent(menu, "popuphidden");
  if (AppConstants.platform === "macosx") {
    // Mocking on OSX, see above.
    if (itemToSelect) {
      itemToSelect.doCommand();
    }
    menu.dispatchEvent(new MouseEvent("popuphiding"));
    menu.dispatchEvent(new MouseEvent("popuphidden"));
  } else if (itemToSelect) {
    EventUtils.synthesizeMouseAtCenter(itemToSelect, {}, win);
  } else {
    menu.hidePopup();
  }
  return hidden;
}

async function openChromeContextMenu(menuId, target, win = window) {
  const node = win.document.querySelector(target);
  const menu = win.document.getElementById(menuId);
  const shown = BrowserTestUtils.waitForEvent(menu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(node, { type: "contextmenu" }, win);
  await shown;
  return menu;
}

async function openSubmenu(submenuItem, win = window) {
  const submenu = submenuItem.menupopup;
  const shown = BrowserTestUtils.waitForEvent(submenu, "popupshown");
  submenuItem.openMenu(true);
  await shown;
  return submenu;
}

function closeChromeContextMenu(menuId, itemToSelect, win = window) {
  const menu = win.document.getElementById(menuId);
  const hidden = BrowserTestUtils.waitForEvent(menu, "popuphidden");
  if (itemToSelect) {
    itemToSelect.closest("menupopup").activateItem(itemToSelect);
  } else {
    menu.hidePopup();
  }
  return hidden;
}

async function openActionContextMenu(extension, kind, win = window) {
  // See comment from getPageActionButton below.
  win.gURLBar.setPageProxyState("valid");
  await promiseAnimationFrame(win);
  let buttonID;
  let menuID;
  if (kind == "page") {
    buttonID =
      "#" +
      BrowserPageActions.urlbarButtonNodeIDForActionID(
        makeWidgetId(extension.id)
      );
    menuID = "pageActionContextMenu";
  } else {
    buttonID = `#${makeWidgetId(extension.id)}-${kind}-action`;
    menuID = "toolbar-context-menu";
  }
  return openChromeContextMenu(menuID, buttonID, win);
}

function closeActionContextMenu(itemToSelect, kind, win = window) {
  let menuID =
    kind == "page" ? "pageActionContextMenu" : "toolbar-context-menu";
  return closeChromeContextMenu(menuID, itemToSelect, win);
}

function openTabContextMenu(tab = gBrowser.selectedTab) {
  // The TabContextMenu initializes its strings only on a focus or mouseover event.
  // Calls focus event on the TabContextMenu before opening.
  tab.focus();
  let indexOfTab = Array.prototype.indexOf.call(tab.parentNode.children, tab);
  return openChromeContextMenu(
    "tabContextMenu",
    `.tabbrowser-tab:nth-child(${indexOfTab + 1})`,
    tab.ownerGlobal
  );
}

function closeTabContextMenu(itemToSelect, win = window) {
  return closeChromeContextMenu("tabContextMenu", itemToSelect, win);
}

function getPageActionPopup(extension, win = window) {
  return AppUiTestInternals.getPageActionPopup(win, extension.id);
}

function getPageActionButton(extension, win = window) {
  return AppUiTestInternals.getPageActionButton(win, extension.id);
}

function clickPageAction(extension, win = window, modifiers = {}) {
  return AppUiTestDelegate.clickPageAction(win, extension.id, modifiers);
}

// Shows the popup for the page action which for lists
// all available page actions
async function showPageActionsPanel(win = window) {
  // See the comment at getPageActionButton
  win.gURLBar.setPageProxyState("valid");
  await promiseAnimationFrame(win);

  let pageActionsPopup = win.document.getElementById("pageActionPanel");

  let popupShownPromise = promisePopupShown(pageActionsPopup);
  EventUtils.synthesizeMouseAtCenter(
    win.document.getElementById("pageActionButton"),
    {},
    win
  );
  await popupShownPromise;

  return pageActionsPopup;
}

async function clickPageActionInPanel(extension, win = window, modifiers = {}) {
  let pageActionsPopup = await showPageActionsPanel(win);

  let pageActionId = BrowserPageActions.panelButtonNodeIDForActionID(
    makeWidgetId(extension.id)
  );

  let popupHiddenPromise = promisePopupHidden(pageActionsPopup);
  let widgetButton = win.document.getElementById(pageActionId);
  EventUtils.synthesizeMouseAtCenter(widgetButton, modifiers, win);
  if (widgetButton.disabled) {
    pageActionsPopup.hidePopup();
  }
  await popupHiddenPromise;

  return new Promise(SimpleTest.executeSoon);
}

async function triggerPageActionWithKeyboard(
  extension,
  modifiers = {},
  win = window
) {
  let elem = await getPageActionButton(extension, win);
  await focusButtonAndPressKey("KEY_Enter", elem, modifiers);
  return new Promise(SimpleTest.executeSoon);
}

async function triggerPageActionWithKeyboardInPanel(
  extension,
  modifiers = {},
  win = window
) {
  let pageActionsPopup = await showPageActionsPanel(win);

  let pageActionId = BrowserPageActions.panelButtonNodeIDForActionID(
    makeWidgetId(extension.id)
  );

  let popupHiddenPromise = promisePopupHidden(pageActionsPopup);
  let widgetButton = win.document.getElementById(pageActionId);
  if (widgetButton.disabled) {
    pageActionsPopup.hidePopup();
    return new Promise(SimpleTest.executeSoon);
  }

  // Use key navigation so that the PanelMultiView doesn't ignore key events
  while (win.document.activeElement != widgetButton) {
    EventUtils.synthesizeKey("KEY_ArrowDown");
    ok(
      pageActionsPopup.contains(win.document.activeElement),
      "Focus is inside of the panel"
    );
  }
  EventUtils.synthesizeKey("KEY_Enter", modifiers);
  await popupHiddenPromise;

  return new Promise(SimpleTest.executeSoon);
}

function closePageAction(extension, win = window) {
  return AppUiTestDelegate.closePageAction(win, extension.id);
}

function promisePrefChangeObserved(pref) {
  return new Promise((resolve, reject) =>
    Preferences.observe(pref, function prefObserver() {
      Preferences.ignore(pref, prefObserver);
      resolve();
    })
  );
}

function promiseWindowRestored(window) {
  return new Promise(resolve =>
    window.addEventListener("SSWindowRestored", resolve, { once: true })
  );
}

function awaitEvent(eventName, id) {
  return new Promise(resolve => {
    let listener = (_eventName, ...args) => {
      let extension = args[0];
      if (_eventName === eventName && extension.id == id) {
        Management.off(eventName, listener);
        resolve();
      }
    };

    Management.on(eventName, listener);
  });
}

function* BrowserWindowIterator() {
  for (let currentWindow of Services.wm.getEnumerator("navigator:browser")) {
    if (!currentWindow.closed) {
      yield currentWindow;
    }
  }
}

async function locationChange(tab, url, task) {
  let locationChanged = BrowserTestUtils.waitForLocationChange(gBrowser, url);
  await SpecialPowers.spawn(tab.linkedBrowser, [url], task);
  return locationChanged;
}

function navigateTab(tab, url) {
  return locationChange(tab, url, url => {
    content.location.href = url;
  });
}

function historyPushState(tab, url) {
  return locationChange(tab, url, url => {
    content.history.pushState(null, null, url);
  });
}

// This monitor extension runs with incognito: not_allowed, if it receives any
// events with incognito data it fails.
async function startIncognitoMonitorExtension() {
  function background() {
    // Bug 1513220 - We're unable to get the tab during onRemoved, so we track
    // valid tabs in "seen" so we can at least validate tabs that we have "seen"
    // during onRemoved.  This means that the monitor extension must be started
    // prior to creating any tabs that will be removed.

    // Map<tabId -> tab>
    let seenTabs = new Map();
    function getTabById(tabId) {
      return seenTabs.has(tabId)
        ? seenTabs.get(tabId)
        : browser.tabs.get(tabId);
    }

    async function testTab(tabOrId, eventName) {
      let tab = tabOrId;
      if (typeof tabOrId == "number") {
        let tabId = tabOrId;
        try {
          tab = await getTabById(tabId);
        } catch (e) {
          browser.test.fail(
            `tabs.${eventName} for id ${tabOrId} unexpected failure ${e}\n`
          );
          return;
        }
      }
      browser.test.assertFalse(
        tab.incognito,
        `tabs.${eventName} ${tab.id}: monitor extension got expected incognito value`
      );
      seenTabs.set(tab.id, tab);
    }
    async function testTabInfo(tabInfo, eventName) {
      if (typeof tabInfo == "number") {
        await testTab(tabInfo, eventName);
      } else if (typeof tabInfo == "object") {
        if (tabInfo.id !== undefined) {
          await testTab(tabInfo, eventName);
        } else if (tabInfo.tab !== undefined) {
          await testTab(tabInfo.tab, eventName);
        } else if (tabInfo.tabIds !== undefined) {
          await Promise.all(
            tabInfo.tabIds.map(tabId => testTab(tabId, eventName))
          );
        } else if (tabInfo.tabId !== undefined) {
          await testTab(tabInfo.tabId, eventName);
        }
      }
    }
    let tabEvents = [
      "onUpdated",
      "onCreated",
      "onAttached",
      "onDetached",
      "onRemoved",
      "onMoved",
      "onZoomChange",
      "onHighlighted",
    ];
    for (let eventName of tabEvents) {
      browser.tabs[eventName].addListener(async details => {
        await testTabInfo(details, eventName);
      });
    }
    browser.tabs.onReplaced.addListener(async (addedTabId, removedTabId) => {
      await testTabInfo(addedTabId, "onReplaced (addedTabId)");
      await testTabInfo(removedTabId, "onReplaced (removedTabId)");
    });

    // Map<windowId -> window>
    let seenWindows = new Map();
    function getWindowById(windowId) {
      return seenWindows.has(windowId)
        ? seenWindows.get(windowId)
        : browser.windows.get(windowId);
    }

    browser.windows.onCreated.addListener(window => {
      browser.test.assertFalse(
        window.incognito,
        `windows.onCreated monitor extension got expected incognito value`
      );
      seenWindows.set(window.id, window);
    });
    browser.windows.onRemoved.addListener(async windowId => {
      let window;
      try {
        window = await getWindowById(windowId);
      } catch (e) {
        browser.test.fail(
          `windows.onCreated for id ${windowId} unexpected failure ${e}\n`
        );
        return;
      }
      browser.test.assertFalse(
        window.incognito,
        `windows.onRemoved ${window.id}: monitor extension got expected incognito value`
      );
    });
    browser.windows.onFocusChanged.addListener(async windowId => {
      if (windowId == browser.windows.WINDOW_ID_NONE) {
        return;
      }
      // onFocusChanged will also fire for blur so check actual window.incognito value.
      let window;
      try {
        window = await getWindowById(windowId);
      } catch (e) {
        browser.test.fail(
          `windows.onFocusChanged for id ${windowId} unexpected failure ${e}\n`
        );
        return;
      }
      browser.test.assertFalse(
        window.incognito,
        `windows.onFocusChanged ${window.id}: monitor extesion got expected incognito value`
      );
      seenWindows.set(window.id, window);
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },
    incognitoOverride: "not_allowed",
    background,
  });
  await extension.startup();
  return extension;
}

async function getIncognitoWindow(url = "about:privatebrowsing") {
  // Since events will be limited based on incognito, we need a
  // spanning extension to get the tab id so we can test access failure.

  function background(expectUrl) {
    browser.tabs.onUpdated.addListener((tabId, changeInfo, tab) => {
      if (changeInfo.status === "complete" && tab.url === expectUrl) {
        browser.test.sendMessage("data", { tabId, windowId: tab.windowId });
      }
    });
  }

  let windowWatcher = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },
    background: `(${background})(${JSON.stringify(url)})`,
    incognitoOverride: "spanning",
  });

  await windowWatcher.startup();
  let data = windowWatcher.awaitMessage("data");

  let win = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  BrowserTestUtils.loadURI(win.gBrowser.selectedBrowser, url);

  let details = await data;
  await windowWatcher.unload();
  return { win, details };
}

/**
 * Windows 7 and 8 set the window's background-color on :root instead of
 * #navigator-toolbox to avoid bug 1695280. When that bug is fixed, this
 * function and the assertions it gates can be removed.
 *
 * @returns {boolean} True if the window's background-color is set on :root
 *   rather than #navigator-toolbox.
 **/
function backgroundColorSetOnRoot() {
  const os = ClientEnvironmentBase.os;
  if (!os.isWindows) {
    return false;
  }
  return os.windowsVersion < 10;
}
