/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kDownloadAutoHidePref = "browser.download.autohideButton";

registerCleanupFunction(async function() {
  Services.prefs.clearUserPref(kDownloadAutoHidePref);
  if (document.documentElement.hasAttribute("customizing")) {
    await gCustomizeMode.reset();
    await promiseCustomizeEnd();
  } else {
    CustomizableUI.reset();
  }
});

add_setup(async () => {
  // Disable window occlusion. See bug 1733955 / bug 1779559.
  if (navigator.platform.indexOf("Win") == 0) {
    await SpecialPowers.pushPrefEnv({
      set: [["widget.windows.window_occlusion_tracking.enabled", false]],
    });
  }
});

add_task(async function checkStateDuringPrefFlips() {
  ok(
    Services.prefs.getBoolPref(kDownloadAutoHidePref),
    "Should be autohiding the button by default"
  );
  ok(
    !DownloadsIndicatorView.hasDownloads,
    "Should be no downloads when starting the test"
  );
  let downloadsButton = document.getElementById("downloads-button");
  ok(
    downloadsButton.hasAttribute("hidden"),
    "Button should be hidden in the toolbar"
  );
  await gCustomizeMode.addToPanel(downloadsButton);
  ok(
    !downloadsButton.hasAttribute("hidden"),
    "Button shouldn't be hidden in the panel"
  );
  ok(
    !Services.prefs.getBoolPref(kDownloadAutoHidePref),
    "Pref got set to false when the user moved the button"
  );
  gCustomizeMode.addToToolbar(downloadsButton);
  ok(
    !Services.prefs.getBoolPref(kDownloadAutoHidePref),
    "Pref remains false when the user moved the button"
  );
  Services.prefs.setBoolPref(kDownloadAutoHidePref, true);
  ok(
    downloadsButton.hasAttribute("hidden"),
    "Button should be hidden again in the toolbar " +
      "now that we flipped the pref"
  );
  Services.prefs.setBoolPref(kDownloadAutoHidePref, false);
  ok(
    !downloadsButton.hasAttribute("hidden"),
    "Button shouldn't be hidden with autohide turned off"
  );
  await gCustomizeMode.addToPanel(downloadsButton);
  ok(
    !downloadsButton.hasAttribute("hidden"),
    "Button shouldn't be hidden with autohide turned off " +
      "after moving it to the panel"
  );
  gCustomizeMode.addToToolbar(downloadsButton);
  ok(
    !downloadsButton.hasAttribute("hidden"),
    "Button shouldn't be hidden with autohide turned off " +
      "after moving it back to the toolbar"
  );
  await gCustomizeMode.addToPanel(downloadsButton);
  Services.prefs.setBoolPref(kDownloadAutoHidePref, true);
  ok(
    !downloadsButton.hasAttribute("hidden"),
    "Button should still not be hidden with autohide turned back on " +
      "because it's in the panel"
  );
  // Use CUI directly instead of the customize mode APIs,
  // to avoid tripping the "automatically turn off autohide" code.
  CustomizableUI.addWidgetToArea("downloads-button", "nav-bar");
  ok(
    downloadsButton.hasAttribute("hidden"),
    "Button should be hidden again in the toolbar"
  );
  gCustomizeMode.removeFromArea(downloadsButton);
  Services.prefs.setBoolPref(kDownloadAutoHidePref, false);
  // Can't use gCustomizeMode.addToToolbar here because it doesn't work for
  // palette items if the window isn't in customize mode:
  CustomizableUI.addWidgetToArea(
    downloadsButton.id,
    CustomizableUI.AREA_NAVBAR
  );
  ok(
    !downloadsButton.hasAttribute("hidden"),
    "Button should be unhidden again in the toolbar " +
      "even if the pref was flipped while the button was in the palette"
  );
  Services.prefs.setBoolPref(kDownloadAutoHidePref, true);
});

add_task(async function checkStateInCustomizeMode() {
  ok(
    Services.prefs.getBoolPref("browser.download.autohideButton"),
    "Should be autohiding the button"
  );
  let downloadsButton = document.getElementById("downloads-button");
  await promiseCustomizeStart();
  ok(
    !downloadsButton.hasAttribute("hidden"),
    "Button should be shown in customize mode."
  );
  await promiseCustomizeEnd();
  ok(
    downloadsButton.hasAttribute("hidden"),
    "Button should be hidden if it's in the toolbar " +
      "after customize mode without any moves."
  );
  await promiseCustomizeStart();
  await gCustomizeMode.addToPanel(downloadsButton);
  ok(
    !downloadsButton.hasAttribute("hidden"),
    "Button should be shown in customize mode when moved to the panel"
  );
  gCustomizeMode.addToToolbar(downloadsButton);
  ok(
    !downloadsButton.hasAttribute("hidden"),
    "Button should be shown in customize mode when moved back to the toolbar"
  );
  gCustomizeMode.removeFromArea(downloadsButton);
  ok(
    !downloadsButton.hasAttribute("hidden"),
    "Button should be shown in customize mode when in the palette"
  );
  Services.prefs.setBoolPref(kDownloadAutoHidePref, false);
  Services.prefs.setBoolPref(kDownloadAutoHidePref, true);
  ok(
    !downloadsButton.hasAttribute("hidden"),
    "Button should be shown in customize mode " +
      "even when flipping the autohide pref"
  );
  await gCustomizeMode.addToPanel(downloadsButton);
  await promiseCustomizeEnd();
  ok(
    !downloadsButton.hasAttribute("hidden"),
    "Button should be shown after customize mode when moved to the panel"
  );
  await promiseCustomizeStart();
  gCustomizeMode.addToToolbar(downloadsButton);
  await promiseCustomizeEnd();
  ok(
    !downloadsButton.hasAttribute("hidden"),
    "Button should be shown in the toolbar after " +
      "customize mode because we moved it."
  );
  await promiseCustomizeStart();
  await gCustomizeMode.reset();
  ok(
    !downloadsButton.hasAttribute("hidden"),
    "Button should be shown in the toolbar in customize mode after a reset."
  );
  await gCustomizeMode.undoReset();
  ok(
    !downloadsButton.hasAttribute("hidden"),
    "Button should be shown in the toolbar in customize mode " +
      "when undoing the reset."
  );
  await gCustomizeMode.addToPanel(downloadsButton);
  await gCustomizeMode.reset();
  ok(
    !downloadsButton.hasAttribute("hidden"),
    "Button should be shown in the toolbar in customize mode " +
      "after a reset moved it."
  );
  await gCustomizeMode.undoReset();
  ok(
    !downloadsButton.hasAttribute("hidden"),
    "Button should be shown in the panel in customize mode " +
      "when undoing the reset."
  );
  await gCustomizeMode.reset();
  await promiseCustomizeEnd();
});

add_task(async function checkStateInCustomizeModeMultipleWindows() {
  ok(
    Services.prefs.getBoolPref("browser.download.autohideButton"),
    "Should be autohiding the button"
  );
  let downloadsButton = document.getElementById("downloads-button");
  await promiseCustomizeStart();
  ok(
    !downloadsButton.hasAttribute("hidden"),
    "Button should be shown in customize mode."
  );
  let otherWin = await BrowserTestUtils.openNewBrowserWindow();
  let otherDownloadsButton = otherWin.document.getElementById(
    "downloads-button"
  );
  ok(
    otherDownloadsButton.hasAttribute("hidden"),
    "Button should be hidden in the other window."
  );

  // Use CUI directly instead of the customize mode APIs,
  // to avoid tripping the "automatically turn off autohide" code.
  CustomizableUI.addWidgetToArea(
    "downloads-button",
    CustomizableUI.AREA_FIXED_OVERFLOW_PANEL
  );
  ok(
    !downloadsButton.hasAttribute("hidden"),
    "Button should still be shown in customize mode."
  );
  ok(
    !otherDownloadsButton.hasAttribute("hidden"),
    "Button should be shown in the other window too because it's in a panel."
  );

  CustomizableUI.addWidgetToArea(
    "downloads-button",
    CustomizableUI.AREA_NAVBAR
  );
  ok(
    !downloadsButton.hasAttribute("hidden"),
    "Button should still be shown in customize mode."
  );
  ok(
    otherDownloadsButton.hasAttribute("hidden"),
    "Button should be hidden again in the other window."
  );

  Services.prefs.setBoolPref(kDownloadAutoHidePref, false);
  ok(
    !downloadsButton.hasAttribute("hidden"),
    "Button should be shown in customize mode"
  );
  ok(
    !otherDownloadsButton.hasAttribute("hidden"),
    "Button should be shown in the other window with the pref flipped"
  );

  Services.prefs.setBoolPref(kDownloadAutoHidePref, true);
  ok(
    !downloadsButton.hasAttribute("hidden"),
    "Button should be shown in customize mode " +
      "even when flipping the autohide pref"
  );
  ok(
    otherDownloadsButton.hasAttribute("hidden"),
    "Button should be hidden in the other window with the pref flipped again"
  );

  await gCustomizeMode.addToPanel(downloadsButton);
  ok(
    !downloadsButton.hasAttribute("hidden"),
    "Button should still be shown in customize mode."
  );
  ok(
    !otherDownloadsButton.hasAttribute("hidden"),
    "Button should be shown in the other window too because it's in a panel."
  );

  gCustomizeMode.removeFromArea(downloadsButton);
  ok(
    !Services.prefs.getBoolPref(kDownloadAutoHidePref),
    "Autohide pref turned off by moving the button"
  );
  ok(
    !downloadsButton.hasAttribute("hidden"),
    "Button should still be shown in customize mode."
  );
  // Don't need to assert in the other window - button is gone there.

  await gCustomizeMode.reset();
  ok(
    Services.prefs.getBoolPref(kDownloadAutoHidePref),
    "Autohide pref reset by reset()"
  );
  ok(
    !downloadsButton.hasAttribute("hidden"),
    "Button should still be shown in customize mode."
  );
  ok(
    otherDownloadsButton.hasAttribute("hidden"),
    "Button should be hidden in the other window."
  );
  ok(
    otherDownloadsButton.closest("#nav-bar"),
    "Button should be back in the nav bar in the other window."
  );

  await promiseCustomizeEnd();
  ok(
    downloadsButton.hasAttribute("hidden"),
    "Button should be hidden again outside of customize mode"
  );
  await BrowserTestUtils.closeWindow(otherWin);
});

add_task(async function checkStateForDownloads() {
  ok(
    Services.prefs.getBoolPref("browser.download.autohideButton"),
    "Should be autohiding the button"
  );
  let downloadsButton = document.getElementById("downloads-button");
  ok(
    downloadsButton.hasAttribute("hidden"),
    "Button should be hidden when there are no downloads."
  );

  await task_addDownloads([{ state: DownloadsCommon.DOWNLOAD_DOWNLOADING }]);
  ok(
    !downloadsButton.hasAttribute("hidden"),
    "Button should be unhidden when there are downloads."
  );
  let publicList = await Downloads.getList(Downloads.PUBLIC);
  let downloads = await publicList.getAll();
  for (let download of downloads) {
    publicList.remove(download);
  }
  ok(
    downloadsButton.hasAttribute("hidden"),
    "Button should be hidden when the download is removed"
  );
  await task_addDownloads([{ state: DownloadsCommon.DOWNLOAD_DOWNLOADING }]);
  ok(
    !downloadsButton.hasAttribute("hidden"),
    "Button should be unhidden when there are downloads."
  );

  Services.prefs.setBoolPref(kDownloadAutoHidePref, false);
  ok(
    !downloadsButton.hasAttribute("hidden"),
    "Button should still be unhidden."
  );

  downloads = await publicList.getAll();
  for (let download of downloads) {
    publicList.remove(download);
  }
  ok(
    !downloadsButton.hasAttribute("hidden"),
    "Button should still be unhidden because the pref was flipped."
  );
  Services.prefs.setBoolPref(kDownloadAutoHidePref, true);
  ok(
    downloadsButton.hasAttribute("hidden"),
    "Button should be hidden now that the pref flipped back " +
      "because there were already no downloads."
  );

  gCustomizeMode.addToPanel(downloadsButton);
  ok(
    !downloadsButton.hasAttribute("hidden"),
    "Button should not be hidden in the panel."
  );

  await task_addDownloads([{ state: DownloadsCommon.DOWNLOAD_DOWNLOADING }]);

  downloads = await publicList.getAll();
  for (let download of downloads) {
    publicList.remove(download);
  }

  ok(
    !downloadsButton.hasAttribute("hidden"),
    "Button should still not be hidden in the panel " +
      "when downloads count reaches 0 after being non-0."
  );

  CustomizableUI.reset();
});

/**
 * Check that if the button is moved to the palette, we unhide it
 * in customize mode even if it was always hidden. We use a new
 * window to test this.
 */
add_task(async function checkStateWhenHiddenInPalette() {
  ok(
    Services.prefs.getBoolPref(kDownloadAutoHidePref),
    "Pref should be causing us to autohide"
  );
  gCustomizeMode.removeFromArea(document.getElementById("downloads-button"));
  // In a new window, the button will have been hidden
  let otherWin = await BrowserTestUtils.openNewBrowserWindow();
  ok(
    !otherWin.document.getElementById("downloads-button"),
    "Button shouldn't be visible in the window"
  );

  let paletteButton = otherWin.gNavToolbox.palette.querySelector(
    "#downloads-button"
  );
  ok(paletteButton, "Button should exist in the palette");
  if (paletteButton) {
    ok(paletteButton.hidden, "Button will still have the hidden attribute");
    await promiseCustomizeStart(otherWin);
    ok(
      !paletteButton.hidden,
      "Button should no longer be hidden in customize mode"
    );
    ok(
      otherWin.document.getElementById("downloads-button"),
      "Button should be in the document now."
    );
    await promiseCustomizeEnd(otherWin);
    // We purposefully don't assert anything about what happens next.
    // It doesn't really matter if the button remains unhidden in
    // the palette, and if we move it we'll unhide it then (the other
    // tests check this).
  }
  await BrowserTestUtils.closeWindow(otherWin);
  CustomizableUI.reset();
});

add_task(async function checkContextMenu() {
  let contextMenu = document.getElementById("toolbar-context-menu");
  let checkbox = document.getElementById(
    "toolbar-context-autohide-downloads-button"
  );
  let button = document.getElementById("downloads-button");

  is(
    Services.prefs.getBoolPref(kDownloadAutoHidePref),
    true,
    "Pref should be causing us to autohide"
  );
  is(
    DownloadsIndicatorView.hasDownloads,
    false,
    "Should be no downloads when starting the test"
  );
  is(button.hidden, true, "Downloads button is hidden");

  info("Simulate a download to show the downloads button.");
  DownloadsIndicatorView.hasDownloads = true;
  is(button.hidden, false, "Downloads button is visible");

  info("Check context menu");
  await openContextMenu(button);
  is(checkbox.hidden, false, "Auto-hide checkbox is visible");
  is(checkbox.getAttribute("checked"), "true", "Auto-hide is enabled");

  info("Disable auto-hide via context menu");
  clickCheckbox(checkbox);
  is(
    Services.prefs.getBoolPref(kDownloadAutoHidePref),
    false,
    "Pref has been set to false"
  );

  info("Clear downloads");
  DownloadsIndicatorView.hasDownloads = false;
  is(button.hidden, false, "Downloads button is still visible");

  info("Check context menu");
  await openContextMenu(button);
  is(checkbox.hidden, false, "Auto-hide checkbox is visible");
  is(checkbox.hasAttribute("checked"), false, "Auto-hide is disabled");

  info("Enable auto-hide via context menu");
  clickCheckbox(checkbox);
  is(button.hidden, true, "Downloads button is hidden");
  is(
    Services.prefs.getBoolPref(kDownloadAutoHidePref),
    true,
    "Pref has been set to true"
  );

  info("Check context menu in another button");
  await openContextMenu(document.getElementById("reload-button"));
  is(checkbox.hidden, true, "Auto-hide checkbox is hidden");
  contextMenu.hidePopup();

  info("Open popup directly");
  contextMenu.openPopup();
  is(checkbox.hidden, true, "Auto-hide checkbox is hidden");
  contextMenu.hidePopup();
});

function promiseCustomizeStart(aWindow = window) {
  return new Promise(resolve => {
    aWindow.gNavToolbox.addEventListener("customizationready", resolve, {
      once: true,
    });
    aWindow.gCustomizeMode.enter();
  });
}

function promiseCustomizeEnd(aWindow = window) {
  return new Promise(resolve => {
    aWindow.gNavToolbox.addEventListener("aftercustomization", resolve, {
      once: true,
    });
    aWindow.gCustomizeMode.exit();
  });
}

function clickCheckbox(checkbox) {
  // Clicking a checkbox toggles its checkedness first.
  if (checkbox.getAttribute("checked") == "true") {
    checkbox.removeAttribute("checked");
  } else {
    checkbox.setAttribute("checked", "true");
  }
  // Then it runs the command and closes the popup.
  checkbox.doCommand();
  checkbox.parentElement.hidePopup();
}
