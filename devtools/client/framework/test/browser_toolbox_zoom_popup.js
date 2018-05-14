/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the popup menu position when zooming in the devtools panel.

const {Toolbox} = require("devtools/client/framework/toolbox");

// Use simple URL in order to prevent displacing the left positon of frames menu.
const TEST_URL = "data:text/html;charset=utf-8,<iframe/>";

add_task(async function() {
  registerCleanupFunction(async function() {
    Services.prefs.clearUserPref("devtools.toolbox.zoomValue");
  });
  const zoom = 1.5;
  Services.prefs.setCharPref("devtools.toolbox.zoomValue", zoom.toString(10));

  info("Load iframe page for checking the frame menu with x1.5 zoom.");
  await addTab(TEST_URL);
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let toolbox = await gDevTools.showToolbox(target,
                                            "inspector",
                                            Toolbox.HostType.WINDOW);
  let inspector = toolbox.getCurrentPanel();
  let hostWindow = toolbox.win.parent;
  let originWidth = hostWindow.outerWidth;
  let originHeight = hostWindow.outerHeight;
  let windowUtils = toolbox.win.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindowUtils);

  info("Waiting for the toolbox window will to be rendered with zoom x1.5");
  await waitUntil(() => {
    return parseFloat(windowUtils.fullZoom.toFixed(1)) === parseFloat(zoom);
  });

  info("Resizing and moving the toolbox window in order to display the chevron menu.");
  // If window is displayed bottom of screen, menu might be displayed above of button.
  hostWindow.moveTo(0, 0);

  // This size will display inspector's tabs menu button and chevron menu button of toolbox.
  let prevTabs = toolbox.doc.querySelectorAll(".devtools-tab").length;
  hostWindow.resizeTo(400, hostWindow.outerHeight);
  await waitUntil(() => {
    return hostWindow.screen.top === 0 &&
      hostWindow.screen.left === 0 &&
      hostWindow.outerWidth === 400 &&
      toolbox.doc.getElementById("tools-chevron-menu-button") &&
      inspector.panelDoc.querySelector(".all-tabs-menu") &&
      prevTabs != toolbox.doc.querySelectorAll(".devtools-tab").length;
  });

  let menuList =
    [toolbox.win.document.getElementById("toolbox-meatball-menu-button"),
     toolbox.win.document.getElementById("command-button-frames"),
     toolbox.win.document.getElementById("tools-chevron-menu-button"),
     inspector.panelDoc.querySelector(".all-tabs-menu")];

  for (const menu of menuList) {
    let [btnRect, menuRect] = await getButtonAndMenuRects(toolbox, menu);

    // Allow rounded error and platform offset value.
    // horizontal : eIntID_ContextMenuOffsetHorizontal of GTK and Windows uses 2.
    // vertical: eIntID_ContextMenuOffsetVertical of macOS uses -6.
    let xDelta = Math.abs(menuRect.left - btnRect.left);
    let yDelta = Math.abs(menuRect.top - btnRect.bottom);
    ok(xDelta < 2, "xDelta is lower than 2: " + xDelta + ". #" + menu.id);
    ok(yDelta < 6, "yDelta is lower than 6: " + yDelta + ". #" + menu.id);
  }

  let onResize = once(hostWindow, "resize");
  hostWindow.resizeTo(originWidth, originHeight);
  await onResize;

  await toolbox.destroy();
  gBrowser.removeCurrentTab();
});

/**
 * Getting the rectangle of the menu button and popup menu.
 *  - The menu button rectangle will be calculated from specified button.
 *  - The popup menu rectangle will be calculated from displayed popup menu
 *    which clicking the specified button.
 */
async function getButtonAndMenuRects(toolbox, menuButton) {
  info("Show popup menu with click event.");
  menuButton.click();

  let popupset = toolbox.doc.querySelector("popupset");
  let menuPopup;
  await waitUntil(() => {
    menuPopup = popupset.querySelector("menupopup[menu-api=\"true\"]");
    return !!menuPopup && menuPopup.state === "open";
  });
  ok(menuPopup, "Menu popup is displayed.");

  let btnRect = menuButton.getBoxQuads({relativeTo: toolbox.doc})[0].bounds;
  let menuRect = menuPopup.getBoxQuads({relativeTo: toolbox.doc})[0].bounds;

  info("Hide popup menu.");
  let onPopupHidden = once(menuPopup, "popuphidden");
  menuPopup.hidePopup();
  await onPopupHidden;

  return [btnRect, menuRect];
}
