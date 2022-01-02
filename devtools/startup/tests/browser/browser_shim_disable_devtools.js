/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* eslint-env browser */

const { require } = ChromeUtils.import(
  "resource://devtools/shared/loader/Loader.jsm"
);
const { CustomizableUI } = ChromeUtils.import(
  "resource:///modules/CustomizableUI.jsm"
);
const { AppConstants } = require("resource://gre/modules/AppConstants.jsm");
const { gDevTools } = require("devtools/client/framework/devtools");

async function simulateMenuOpen(menu) {
  return new Promise(resolve => {
    menu.addEventListener("popupshown", resolve, { once: true });
    menu.dispatchEvent(new MouseEvent("popupshowing"));
    menu.dispatchEvent(new MouseEvent("popupshown"));
  });
}

async function simulateMenuClosed(menu) {
  return new Promise(resolve => {
    menu.addEventListener("popuphidden", resolve, { once: true });
    menu.dispatchEvent(new MouseEvent("popuphiding"));
    menu.dispatchEvent(new MouseEvent("popuphidden"));
  });
}

/**
 * Test that the preference devtools.policy.disabled disables entry points for devtools.
 */
add_task(async function() {
  info(
    "Disable DevTools entry points (does not apply to the already created window"
  );
  await new Promise(resolve => {
    const options = { set: [["devtools.policy.disabled", true]] };
    SpecialPowers.pushPrefEnv(options, resolve);
  });

  // In DEV_EDITION the browser starts with the developer-button in the toolbar. This
  // applies to all new windows and forces creating keyboard shortcuts. The preference
  // tested should not change without restart, but for the needs of the test, remove the
  // developer-button from the UI before opening a new window.
  if (AppConstants.MOZ_DEV_EDITION) {
    CustomizableUI.removeWidgetFromArea("developer-button");
  }

  info(
    "Open a new window, all window-specific hooks for DevTools will be disabled."
  );
  const win = OpenBrowserWindow({ private: false });
  await waitForDelayedStartupFinished(win);

  info(
    "Open a new tab on the new window to ensure the focus is on the new window"
  );
  const tab = BrowserTestUtils.addTab(
    win.gBrowser,
    "data:text/html;charset=utf-8,<title>foo</title>"
  );
  await BrowserTestUtils.browserLoaded(win.gBrowser.getBrowserForTab(tab));

  info(
    "Synthesize a DevTools shortcut, the toolbox should not open on this new window."
  );
  synthesizeToggleToolboxKey(win);

  // There is no event to wait for here as this shortcut should have no effect.
  /* eslint-disable mozilla/no-arbitrary-setTimeout */
  await new Promise(r => setTimeout(r, 1000));

  is(gDevTools._toolboxes.size, 0, "No toolbox has been opened");

  const browser = gBrowser.selectedTab.linkedBrowser;
  const location = browser.documentURI.spec;
  ok(
    !location.startsWith("about:devtools"),
    "The current tab is not about:devtools"
  );

  info("Open the context menu for the content page.");
  const contextMenu = win.document.getElementById("contentAreaContextMenu");
  const popupShownPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );
  EventUtils.synthesizeMouseAtCenter(
    win.document.documentElement,
    { type: "contextmenu", button: 2 },
    win
  );
  await popupShownPromise;

  const inspectElementItem = contextMenu.querySelector(`#context-inspect`);
  ok(
    inspectElementItem.hidden,
    "The inspect element item is hidden in the context menu"
  );

  info("Close the context menu");
  const onContextMenuHidden = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popuphidden"
  );
  contextMenu.hidePopup();
  await onContextMenuHidden;

  info("Open the menubar Tools menu");
  const toolsMenuPopup = win.document.getElementById("menu_ToolsPopup");
  const browserToolsMenu = win.document.getElementById("browserToolsMenu");
  ok(
    !browserToolsMenu.hidden,
    "The Browser Tools item of the tools menu is visible"
  );

  await simulateMenuOpen(toolsMenuPopup);
  const subMenu = win.document.getElementById("menuWebDeveloperPopup");

  info("Open the Browser Tools sub-menu");
  await simulateMenuOpen(subMenu);

  const visibleMenuItems = Array.from(
    subMenu.querySelectorAll("menuitem")
  ).filter(item => !item.hidden);

  const { menuitems } = require("devtools/client/menus");
  for (const devtoolsItem of menuitems) {
    ok(
      !visibleMenuItems.some(item => item.id === devtoolsItem.id),
      "DevTools menu item is not visible in the Browser Tools menu"
    );
  }

  info("Close out the menu popups");
  await simulateMenuClosed(subMenu);
  await simulateMenuClosed(toolsMenuPopup);

  win.gBrowser.removeTab(tab);

  info("Close the test window");
  const winClosed = BrowserTestUtils.windowClosed(win);
  win.BrowserTryToCloseWindow();
  await winClosed;
});

function waitForDelayedStartupFinished(win) {
  return new Promise(resolve => {
    Services.obs.addObserver(function observer(subject, topic) {
      if (win == subject) {
        Services.obs.removeObserver(
          observer,
          "browser-delayed-startup-finished"
        );
        resolve();
      }
    }, "browser-delayed-startup-finished");
  });
}

/**
 * Helper to call the toggle devtools shortcut.
 */
function synthesizeToggleToolboxKey(win) {
  info("Trigger the toogle toolbox shortcut");
  if (Services.appinfo.OS == "Darwin") {
    EventUtils.synthesizeKey("i", { accelKey: true, altKey: true }, win);
  } else {
    EventUtils.synthesizeKey("i", { accelKey: true, shiftKey: true }, win);
  }
}
