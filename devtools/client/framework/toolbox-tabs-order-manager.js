/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AddonManager } = require("resource://gre/modules/AddonManager.jsm");
const { gDevTools } = require("devtools/client/framework/devtools");
const Telemetry = require("devtools/client/shared/telemetry");
const TABS_REORDERED_SCALAR = "devtools.toolbox.tabs_reordered";
const PREFERENCE_NAME = "devtools.toolbox.tabsOrder";

/**
 * Manage the order of devtools tabs.
 */
class ToolboxTabsOrderManager {
  constructor(toolbox, onOrderUpdated, panelDefinitions) {
    this.toolbox = toolbox;
    this.onOrderUpdated = onOrderUpdated;
    this.currentPanelDefinitions = panelDefinitions || [];

    this.onMouseDown = this.onMouseDown.bind(this);
    this.onMouseMove = this.onMouseMove.bind(this);
    this.onMouseUp = this.onMouseUp.bind(this);

    Services.prefs.addObserver(PREFERENCE_NAME, this.onOrderUpdated);

    this.telemetry = new Telemetry();
  }

  async destroy() {
    Services.prefs.removeObserver(PREFERENCE_NAME, this.onOrderUpdated);

    // Call mouseUp() to clear the state to prepare for in case a dragging was in progress
    // when the destroy() was called.
    await this.onMouseUp();
  }

  insertBefore(target) {
    const xBefore = this.dragTarget.offsetLeft;
    this.toolboxTabsElement.insertBefore(this.dragTarget, target);
    const xAfter = this.dragTarget.offsetLeft;
    this.dragStartX += xAfter - xBefore;
    this.isOrderUpdated = true;
  }

  isFirstTab(tabElement) {
    return !tabElement.previousSibling;
  }

  isLastTab(tabElement) {
    return (
      !tabElement.nextSibling ||
      tabElement.nextSibling.id === "tools-chevron-menu-button"
    );
  }

  isRTL() {
    return this.toolbox.direction === "rtl";
  }

  async saveOrderPreference() {
    const tabs = [...this.toolboxTabsElement.querySelectorAll(".devtools-tab")];
    const tabIds = tabs.map(tab => tab.dataset.extensionId || tab.dataset.id);
    // Concat the overflowed tabs id since they are not contained in visible tabs.
    // The overflowed tabs cannot be reordered so we just append the id from current
    // panel definitions on their order.
    const overflowedTabIds = this.currentPanelDefinitions
      .filter(definition => !tabs.some(tab => tab.dataset.id === definition.id))
      .map(definition => definition.extensionId || definition.id);
    const currentTabIds = tabIds.concat(overflowedTabIds);
    const dragTargetId =
      this.dragTarget.dataset.extensionId || this.dragTarget.dataset.id;
    const prefIds = getTabsOrderFromPreference();
    const absoluteIds = toAbsoluteOrder(prefIds, currentTabIds, dragTargetId);

    // Remove panel id which is not in panel definitions and addons list.
    const extensions = await AddonManager.getAllAddons();
    const definitions = gDevTools.getToolDefinitionArray();
    const result = absoluteIds.filter(
      id =>
        definitions.find(d => id === (d.extensionId || d.id)) ||
        extensions.find(e => id === e.id)
    );

    Services.prefs.setCharPref(PREFERENCE_NAME, result.join(","));
  }

  setCurrentPanelDefinitions(currentPanelDefinitions) {
    this.currentPanelDefinitions = currentPanelDefinitions;
  }

  onMouseDown(e) {
    if (!e.target.classList.contains("devtools-tab")) {
      return;
    }

    this.dragStartX = e.pageX;
    this.dragTarget = e.target;
    this.previousPageX = e.pageX;
    this.toolboxContainerElement = this.dragTarget.closest(
      "#toolbox-container"
    );
    this.toolboxTabsElement = this.dragTarget.closest(".toolbox-tabs");
    this.isOrderUpdated = false;
    this.eventTarget = this.dragTarget.ownerGlobal.top;

    this.eventTarget.addEventListener("mousemove", this.onMouseMove);
    this.eventTarget.addEventListener("mouseup", this.onMouseUp);

    this.toolboxContainerElement.classList.add("tabs-reordering");
  }

  onMouseMove(e) {
    const diffPageX = e.pageX - this.previousPageX;
    let dragTargetCenterX =
      this.dragTarget.offsetLeft + diffPageX + this.dragTarget.clientWidth / 2;
    let isDragTargetPreviousSibling = false;

    const tabElements = this.toolboxTabsElement.querySelectorAll(
      ".devtools-tab"
    );

    // Calculate the minimum and maximum X-offset that can be valid for the drag target.
    const firstElement = tabElements[0];
    const firstElementCenterX =
      firstElement.offsetLeft + firstElement.clientWidth / 2;
    const lastElement = tabElements[tabElements.length - 1];
    const lastElementCenterX =
      lastElement.offsetLeft + lastElement.clientWidth / 2;
    const max = Math.max(firstElementCenterX, lastElementCenterX);
    const min = Math.min(firstElementCenterX, lastElementCenterX);

    // Normalize the target center X so to remain between the first and last tab.
    dragTargetCenterX = Math.min(max, dragTargetCenterX);
    dragTargetCenterX = Math.max(min, dragTargetCenterX);

    for (const tabElement of tabElements) {
      if (tabElement === this.dragTarget) {
        isDragTargetPreviousSibling = true;
        continue;
      }

      // Is the dragTarget near the center of the other tab?
      const anotherCenterX = tabElement.offsetLeft + tabElement.clientWidth / 2;
      const distanceWithDragTarget = Math.abs(
        dragTargetCenterX - anotherCenterX
      );
      const isReplaceable = distanceWithDragTarget < tabElement.clientWidth / 3;

      if (isReplaceable) {
        const replaceableElement = isDragTargetPreviousSibling
          ? tabElement.nextSibling
          : tabElement;
        this.insertBefore(replaceableElement);
        break;
      }
    }

    let distance = e.pageX - this.dragStartX;

    // To accomodate for RTL locales, we cannot rely on the first/last element of the
    // NodeList. We cannot have negative distances for the leftmost tab, and we cannot
    // have positive distances for the rightmost tab.
    const isFirstTab = this.isFirstTab(this.dragTarget);
    const isLastTab = this.isLastTab(this.dragTarget);
    const isLeftmostTab = this.isRTL() ? isLastTab : isFirstTab;
    const isRightmostTab = this.isRTL() ? isFirstTab : isLastTab;

    if ((isLeftmostTab && distance < 0) || (isRightmostTab && distance > 0)) {
      // If the drag target is already edge of the tabs and the mouse will make the
      // element to move to same direction more, keep the position.
      distance = 0;
    }

    this.dragTarget.style.left = `${distance}px`;
    this.previousPageX = e.pageX;
  }

  async onMouseUp() {
    if (!this.dragTarget) {
      // The case in here has two type:
      // 1. Although destroy method was called, it was not during reordering.
      // 2. Although mouse event occur, destroy method was called during reordering.
      return;
    }

    if (this.isOrderUpdated) {
      await this.saveOrderPreference();

      // Log which tabs reordered. The question we want to answer is:
      // "How frequently are the tabs re-ordered, also which tabs get re-ordered?"
      const toolId =
        this.dragTarget.dataset.extensionId || this.dragTarget.dataset.id;
      this.telemetry.keyedScalarAdd(TABS_REORDERED_SCALAR, toolId, 1);
    }

    this.eventTarget.removeEventListener("mousemove", this.onMouseMove);
    this.eventTarget.removeEventListener("mouseup", this.onMouseUp);

    this.toolboxContainerElement.classList.remove("tabs-reordering");
    this.dragTarget.style.left = null;
    this.dragTarget = null;
    this.toolboxContainerElement = null;
    this.toolboxTabsElement = null;
    this.eventTarget = null;
  }
}

function getTabsOrderFromPreference() {
  const pref = Services.prefs.getCharPref(PREFERENCE_NAME, "");
  return pref ? pref.split(",") : [];
}

function sortPanelDefinitions(definitions) {
  const toolIds = getTabsOrderFromPreference();

  return definitions.sort((a, b) => {
    let orderA = toolIds.indexOf(a.extensionId || a.id);
    let orderB = toolIds.indexOf(b.extensionId || b.id);
    orderA = orderA < 0 ? Number.MAX_VALUE : orderA;
    orderB = orderB < 0 ? Number.MAX_VALUE : orderB;
    return orderA - orderB;
  });
}

/*
 * This function returns absolute tab ids that were merged the both ids that are in
 * preference and tabs.
 * Some tabs added with add-ons etc show/hide depending on conditions.
 * However, all of tabs that include hidden tab always keep the relationship with
 * left side tab, except in case the left tab was target of dragging. If the left
 * tab has been moved, it keeps its relationship with the tab next to it.
 *
 * Case 1: Drag a tab to left
 *   currentTabIds: [T1, T2, T3, T4, T5]
 *   prefIds      : [T1, T2, T3, E1(hidden), T4, T5]
 *   drag T4      : [T1, T2, T4, T3, T5]
 *   result       : [T1, T2, T4, T3, E1, T5]
 *
 * Case 2: Drag a tab to right
 *   currentTabIds: [T1, T2, T3, T4, T5]
 *   prefIds      : [T1, T2, T3, E1(hidden), T4, T5]
 *   drag T2      : [T1, T3, T4, T2, T5]
 *   result       : [T1, T3, E1, T4, T2, T5]
 *
 * Case 3: Hidden tab was left end and drag a tab to left end
 *   currentTabIds: [T1, T2, T3, T4, T5]
 *   prefIds      : [E1(hidden), T1, T2, T3, T4, T5]
 *   drag T4      : [T4, T1, T2, T3, T5]
 *   result       : [E1, T4, T1, T2, T3, T5]
 *
 * Case 4: Hidden tab was right end and drag a tab to right end
 *   currentTabIds: [T1, T2, T3, T4, T5]
 *   prefIds      : [T1, T2, T3, T4, T5, E1(hidden)]
 *   drag T1      : [T2, T3, T4, T5, T1]
 *   result       : [T2, T3, T4, T5, E1, T1]
 *
 * @param Array - prefIds: id array of preference
 * @param Array - currentTabIds: id array of appearanced tabs
 * @param String - dragTargetId: id of dragged target
 * @return Array
 */
function toAbsoluteOrder(prefIds, currentTabIds, dragTargetId) {
  currentTabIds = [...currentTabIds];
  let indexAtCurrentTabs = 0;

  for (const prefId of prefIds) {
    if (prefId === dragTargetId) {
      // do nothing
    } else if (currentTabIds.includes(prefId)) {
      indexAtCurrentTabs = currentTabIds.indexOf(prefId) + 1;
    } else {
      currentTabIds.splice(indexAtCurrentTabs, 0, prefId);
      indexAtCurrentTabs += 1;
    }
  }

  return currentTabIds;
}

module.exports.ToolboxTabsOrderManager = ToolboxTabsOrderManager;
module.exports.sortPanelDefinitions = sortPanelDefinitions;
module.exports.toAbsoluteOrder = toAbsoluteOrder;
