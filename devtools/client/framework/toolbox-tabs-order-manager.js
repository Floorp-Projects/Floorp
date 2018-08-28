/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const Telemetry = require("devtools/client/shared/telemetry");
const TABS_REORDERED_SCALAR = "devtools.toolbox.tabs_reordered";
const PREFERENCE_NAME = "devtools.toolbox.tabsOrder";

/**
 * Manage the order of devtools tabs.
 */
class ToolboxTabsOrderManager {
  constructor(onOrderUpdated) {
    this.onOrderUpdated = onOrderUpdated;
    this.currentPanelDefinitions = [];

    this.onMouseDown = this.onMouseDown.bind(this);
    this.onMouseMove = this.onMouseMove.bind(this);
    this.onMouseUp = this.onMouseUp.bind(this);

    Services.prefs.addObserver(PREFERENCE_NAME, this.onOrderUpdated);

    this.telemetry = new Telemetry();
  }

  destroy() {
    Services.prefs.removeObserver(PREFERENCE_NAME, this.onOrderUpdated);

    // Save the reordering preference, because some tools might be removed.
    const ids =
      this.currentPanelDefinitions.map(definition => definition.extensionId || definition.id);
    const pref = ids.join(",");
    Services.prefs.setCharPref(PREFERENCE_NAME, pref);

    this.onMouseUp();
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
    return !tabElement.nextSibling ||
           tabElement.nextSibling.id === "tools-chevron-menu-button";
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
    this.toolboxContainerElement = this.dragTarget.closest("#toolbox-container");
    this.toolboxTabsElement = this.dragTarget.closest(".toolbox-tabs");
    this.isOrderUpdated = false;
    this.eventTarget = this.dragTarget.ownerGlobal.top;

    this.eventTarget.addEventListener("mousemove", this.onMouseMove);
    this.eventTarget.addEventListener("mouseup", this.onMouseUp);

    this.toolboxContainerElement.classList.add("tabs-reordering");
  }

  onMouseMove(e) {
    const diffPageX = e.pageX - this.previousPageX;
    const dragTargetCenterX =
      this.dragTarget.offsetLeft + diffPageX + this.dragTarget.clientWidth / 2;
    let isDragTargetPreviousSibling = false;

    for (const tabElement of this.toolboxTabsElement.querySelectorAll(".devtools-tab")) {
      if (tabElement === this.dragTarget) {
        isDragTargetPreviousSibling = true;
        continue;
      }

      const anotherCenterX = tabElement.offsetLeft + tabElement.clientWidth / 2;
      const isReplaceable =
        // Is the dragTarget near the center of the other tab?
        Math.abs(dragTargetCenterX - anotherCenterX) < tabElement.clientWidth / 3 ||
        // Has the dragTarget moved before the first tab
        // (mouse moved too fast between two events)
        (this.isFirstTab(tabElement) && dragTargetCenterX < anotherCenterX) ||
        // Has the dragTarget moved after the last tab
        // (mouse moved too fast between two events)
        (this.isLastTab(tabElement) && anotherCenterX < dragTargetCenterX);

      if (isReplaceable) {
        const replaceableElement =
          isDragTargetPreviousSibling ? tabElement.nextSibling : tabElement;
        this.insertBefore(replaceableElement);
        break;
      }
    }

    let distance = e.pageX - this.dragStartX;

    if ((this.isFirstTab(this.dragTarget) && distance < 0) ||
        (this.isLastTab(this.dragTarget) && distance > 0)) {
      // If the drag target is already edge of the tabs and the mouse will make the
      // element to move to same direction more, keep the position.
      distance = 0;
    }

    this.dragTarget.style.left = `${ distance }px`;
    this.previousPageX = e.pageX;
  }

  onMouseUp() {
    if (!this.dragTarget) {
      // The case in here has two type:
      // 1. Although destroy method was called, it was not during reordering.
      // 2. Although mouse event occur, destroy method was called during reordering.
      return;
    }

    if (this.isOrderUpdated) {
      const tabs = [...this.toolboxTabsElement.querySelectorAll(".devtools-tab")];
      const tabIds = tabs.map(tab => tab.dataset.extensionId || tab.dataset.id);
      // Concat the overflowed tabs id since they are not contained in visible tabs.
      // The overflowed tabs cannot be reordered so we just append the id from current
      // panel definitions on their order.
      const overflowedTabIds =
        this.currentPanelDefinitions
            .filter(definition => !tabs.some(tab => tab.dataset.id === definition.id))
            .map(definition => definition.extensionId || definition.id);
      const pref = tabIds.concat(overflowedTabIds).join(",");
      Services.prefs.setCharPref(PREFERENCE_NAME, pref);

      // Log which tabs reordered. The question we want to answer is:
      // "How frequently are the tabs re-ordered, also which tabs get re-ordered?"
      const toolId = this.dragTarget.dataset.extensionId || this.dragTarget.dataset.id;
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

function sortPanelDefinitions(definitions) {
  const pref = Services.prefs.getCharPref(PREFERENCE_NAME, "");
  const toolIds = pref.split(",");

  return definitions.sort((a, b) => {
    let orderA = toolIds.indexOf(a.extensionId || a.id);
    let orderB = toolIds.indexOf(b.extensionId || b.id);
    orderA = orderA < 0 ? Number.MAX_VALUE : orderA;
    orderB = orderB < 0 ? Number.MAX_VALUE : orderB;
    return orderA - orderB;
  });
}

module.exports.ToolboxTabsOrderManager = ToolboxTabsOrderManager;
module.exports.sortPanelDefinitions = sortPanelDefinitions;
