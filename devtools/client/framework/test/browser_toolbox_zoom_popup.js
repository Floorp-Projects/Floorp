/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the popup menu position when zooming in the devtools panel.

const { Toolbox } = require("devtools/client/framework/toolbox");
const { getCurrentZoom } = require("devtools/shared/layout/utils");

// Use a simple URL in order to prevent displacing the left position of the
// frames menu.
const TEST_URL = "data:text/html;charset=utf-8,<iframe/>";

add_task(async function() {
  registerCleanupFunction(async function() {
    Services.prefs.clearUserPref("devtools.toolbox.zoomValue");
  });
  const zoom = 1.4;
  Services.prefs.setCharPref("devtools.toolbox.zoomValue", zoom.toString(10));

  info("Load iframe page for checking the frame menu with x1.4 zoom.");
  await addTab(TEST_URL);
  const tab = gBrowser.selectedTab;
  const toolbox = await gDevTools.showToolboxForTab(tab, {
    toolId: "inspector",
    hostType: Toolbox.HostType.WINDOW,
  });
  const inspector = toolbox.getCurrentPanel();
  const hostWindow = toolbox.win.parent;
  const originWidth = hostWindow.outerWidth;
  const originHeight = hostWindow.outerHeight;
  const windowUtils = toolbox.win.windowUtils;

  info("Waiting for the toolbox window will to be rendered with zoom x1.4");
  await waitUntil(() => {
    return parseFloat(windowUtils.fullZoom.toFixed(1)) === zoom;
  });

  info(
    "Resizing and moving the toolbox window in order to display the chevron menu."
  );
  // If the window is displayed bottom of screen, the menu might be displayed
  // above the button so move it to the top of the screen first.
  await moveWindowTo(hostWindow, 0, 0);

  // Shrink the width of the window such that the inspector's tab menu button
  // and chevron button are visible.
  const prevTabs = toolbox.doc.querySelectorAll(".devtools-tab").length;
  hostWindow.resizeTo(400, hostWindow.outerHeight);
  await waitUntil(() => {
    return (
      hostWindow.outerWidth === 400 &&
      toolbox.doc.getElementById("tools-chevron-menu-button") &&
      inspector.panelDoc.querySelector(".all-tabs-menu") &&
      prevTabs != toolbox.doc.querySelectorAll(".devtools-tab").length
    );
  });

  const menuList = [
    toolbox.win.document.getElementById("toolbox-meatball-menu-button"),
    toolbox.win.document.getElementById("command-button-frames"),
    toolbox.win.document.getElementById("tools-chevron-menu-button"),
    inspector.panelDoc.querySelector(".all-tabs-menu"),
  ];

  for (const menu of menuList) {
    const {
      buttonBounds,
      menuType,
      menuBounds,
      arrowBounds,
    } = await getButtonAndMenuInfo(toolbox, menu);

    switch (menuType) {
      case "native":
        {
          // Allow rounded error and platform offset value.
          // horizontal : IntID::ContextMenuOffsetHorizontal of GTK and Windows
          //              uses 2.
          // vertical: IntID::ContextMenuOffsetVertical of macOS uses -6.
          const xDelta = Math.abs(menuBounds.left - buttonBounds.left);
          const yDelta = Math.abs(menuBounds.top - buttonBounds.bottom);
          ok(xDelta < 2, "xDelta is lower than 2: " + xDelta + ". #" + menu.id);
          ok(yDelta < 6, "yDelta is lower than 6: " + yDelta + ". #" + menu.id);
        }
        break;

      case "doorhanger":
        {
          // Calculate the center of the button and center of the arrow and
          // check they align.
          const buttonCenter = buttonBounds.left + buttonBounds.width / 2;
          const arrowCenter = arrowBounds.left + arrowBounds.width / 2;
          const delta = Math.abs(arrowCenter - buttonCenter);
          ok(
            Math.round(delta) <= 1,
            "Center of arrow is within 1px of button center" +
              ` (delta: ${delta})`
          );
        }
        break;
    }
  }

  const onResize = once(hostWindow, "resize");
  hostWindow.resizeTo(originWidth, originHeight);
  await onResize;

  await toolbox.destroy();
  gBrowser.removeCurrentTab();
});

function convertScreenToDoc(rect, doc) {
  const zoom = getCurrentZoom(doc);
  const screenX = doc.defaultView.mozInnerScreenX;
  const screenY = doc.defaultView.mozInnerScreenY;
  return new DOMRect(
    rect.x / zoom - screenX,
    rect.y / zoom - screenY,
    rect.width / zoom,
    rect.height / zoom
  );
}

/**
 * Get the bounds of a menu button and its popup panel. The popup panel is
 * measured by clicking the menu button and looking for its panel (and then
 * hiding it again).
 *
 * @param {Object} doc
 *        The toolbox document to query.
 * @param {Object} menuButton
 *        The button whose size and popup size we should measure.
 * @return {Object}
 *         An object with the following properties:
 *         - buttonBounds {DOMRect} Bounds of the button.
 *         - menuType {string} Type of the menu, "native" or "doorhanger".
 *         - menuBounds {DOMRect} Bounds of the menu panel.
 *         - arrowBounds {DOMRect|null} Bounds of the arrow. Only set when
 *                       menuType is "doorhanger", null otherwise.
 */
async function getButtonAndMenuInfo(toolbox, menuButton) {
  const { doc, topDoc } = toolbox;
  info("Show popup menu with click event.");
  AccessibilityUtils.setEnv({
    // Keyboard accessibility is handled on the toolbox toolbar container level.
    // Users can use arrow keys to navigate between and select tabs.
    nonNegativeTabIndexRule: false,
  });
  EventUtils.sendMouseEvent(
    {
      type: "click",
      screenX: 1,
    },
    menuButton,
    doc.defaultView
  );
  AccessibilityUtils.resetEnv();

  let menuPopup;
  let menuType;
  let menuBounds = null;
  let arrowBounds = null;
  if (menuButton.hasAttribute("aria-controls")) {
    menuType = "doorhanger";
    menuPopup = doc.getElementById(menuButton.getAttribute("aria-controls"));
    await waitUntil(() => menuPopup.classList.contains("tooltip-visible"));
    // menuPopup can be a non-menupopup element, e.g. div. Call getBoxQuads to
    // get its bounds.
    menuBounds = menuPopup.getBoxQuads({ relativeTo: doc })[0].getBounds();
  } else {
    menuType = "native";
    await waitUntil(() => {
      const popupset = topDoc.querySelector("popupset");
      menuPopup = popupset?.querySelector('menupopup[menu-api="true"]');
      return menuPopup?.state === "open";
    });
    // menuPopup is a XUL menupopup element. Call getOuterScreenRect(), which is
    // suported on both native and non-native menupopup implementations.
    menuBounds = convertScreenToDoc(menuPopup.getOuterScreenRect(), doc);
  }
  ok(menuPopup, "Menu popup is displayed.");

  const buttonBounds = menuButton
    .getBoxQuads({ relativeTo: doc })[0]
    .getBounds();

  if (menuType === "doorhanger") {
    const arrow = menuPopup.querySelector(".tooltip-arrow");
    arrowBounds = arrow.getBoxQuads({ relativeTo: doc })[0].getBounds();
  }

  info("Hide popup menu.");
  if (menuType === "doorhanger") {
    EventUtils.sendKey("Escape", doc.defaultView);
    await waitUntil(() => !menuPopup.classList.contains("tooltip-visible"));
  } else {
    const popupHidden = once(menuPopup, "popuphidden");
    menuPopup.hidePopup();
    await popupHidden;
  }

  return { buttonBounds, menuType, menuBounds, arrowBounds };
}
