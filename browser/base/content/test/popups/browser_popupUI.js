/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function toolbar_ui_visibility() {
  SpecialPowers.pushPrefEnv({ set: [["dom.disable_open_during_load", false]] });

  let popupOpened = BrowserTestUtils.waitForNewWindow({ url: "about:blank" });
  BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "data:text/html,<html><script>popup=open('about:blank','','width=300,height=200')</script>"
  );
  const win = await popupOpened;
  const doc = win.document;

  ok(win.gURLBar, "location bar exists in the popup");
  isnot(win.gURLBar.clientWidth, 0, "location bar is visible in the popup");
  ok(win.gURLBar.readOnly, "location bar is read-only in the popup");
  isnot(
    doc.getElementById("Browser:OpenLocation").getAttribute("disabled"),
    "true",
    "'open location' command is not disabled in the popup"
  );

  EventUtils.synthesizeKey("t", { accelKey: true }, win);
  is(
    win.gBrowser.browsers.length,
    1,
    "Accel+T doesn't open a new tab in the popup"
  );
  is(
    gBrowser.browsers.length,
    3,
    "Accel+T opened a new tab in the parent window"
  );
  gBrowser.removeCurrentTab();

  EventUtils.synthesizeKey("w", { accelKey: true }, win);
  ok(win.closed, "Accel+W closes the popup");

  if (!win.closed) {
    win.close();
  }
  gBrowser.removeCurrentTab();
});

add_task(async function titlebar_buttons_visibility() {
  if (!navigator.platform.startsWith("Win")) {
    ok(true, "Testing only on Windows");
    return;
  }

  const BUTTONS_MAY_VISIBLE = true;
  const BUTTONS_NEVER_VISIBLE = false;

  // Always open a new window.
  // With default behavior, it opens a new tab, that doesn't affect button
  // visibility at all.
  Services.prefs.setIntPref("browser.link.open_newwindow", 2);

  const drawInTitlebarValues = [
    [true, BUTTONS_MAY_VISIBLE],
    [false, BUTTONS_NEVER_VISIBLE],
  ];
  const windowFeaturesValues = [
    // Opens a popup
    ["width=300,height=100", BUTTONS_NEVER_VISIBLE],
    ["toolbar", BUTTONS_NEVER_VISIBLE],
    ["menubar", BUTTONS_NEVER_VISIBLE],
    ["menubar,toolbar", BUTTONS_NEVER_VISIBLE],

    // Opens a new window
    ["", BUTTONS_MAY_VISIBLE],
  ];
  const menuBarShownValues = [true, false];

  for (const [drawInTitlebar, drawInTitlebarButtons] of drawInTitlebarValues) {
    Services.prefs.setBoolPref("browser.tabs.drawInTitlebar", drawInTitlebar);

    for (const [
      windowFeatures,
      windowFeaturesButtons,
    ] of windowFeaturesValues) {
      for (const menuBarShown of menuBarShownValues) {
        CustomizableUI.setToolbarVisibility("toolbar-menubar", menuBarShown);

        const popupPromise = BrowserTestUtils.waitForNewWindow("about:blank");
        BrowserTestUtils.openNewForegroundTab(
          gBrowser,
          `data:text/html;charset=UTF-8,<html><script>window.open("about:blank","","${windowFeatures}")</script>`
        );
        const popupWin = await popupPromise;

        const menubar = popupWin.document.querySelector("#toolbar-menubar");
        const menubarIsShown =
          menubar.getAttribute("autohide") != "true" ||
          menubar.getAttribute("inactive") != "true";
        const buttonsInMenubar = menubar.querySelector(
          ".titlebar-buttonbox-container"
        );
        const buttonsInMenubarShown =
          menubarIsShown &&
          popupWin.getComputedStyle(buttonsInMenubar).display != "none";

        const buttonsInTabbar = popupWin.document.querySelector(
          "#TabsToolbar .titlebar-buttonbox-container"
        );
        const buttonsInTabbarShown =
          popupWin.getComputedStyle(buttonsInTabbar).display != "none";

        const params = `drawInTitlebar=${drawInTitlebar}, windowFeatures=${windowFeatures}, menuBarShown=${menuBarShown}`;
        if (
          drawInTitlebarButtons == BUTTONS_MAY_VISIBLE &&
          windowFeaturesButtons == BUTTONS_MAY_VISIBLE
        ) {
          ok(
            buttonsInMenubarShown || buttonsInTabbarShown,
            `Titlebar buttons should be visible: ${params}`
          );
        } else {
          ok(
            !buttonsInMenubarShown,
            `Titlebar buttons should not be visible: ${params}`
          );
          ok(
            !buttonsInTabbarShown,
            `Titlebar buttons should not be visible: ${params}`
          );
        }

        const closedPopupPromise = BrowserTestUtils.windowClosed(popupWin);
        popupWin.close();
        await closedPopupPromise;
        gBrowser.removeCurrentTab();
      }
    }
  }

  CustomizableUI.setToolbarVisibility("toolbar-menubar", false);
  Services.prefs.clearUserPref("browser.tabs.drawInTitlebar");
  Services.prefs.clearUserPref("browser.link.open_newwindow");
});
