/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { AccessibilityFront } = require("devtools/shared/fronts/accessibility");
const EventEmitter = require("devtools/shared/event-emitter");

const Telemetry = require("devtools/client/shared/telemetry");

const { Picker } = require("./picker");
const { A11Y_SERVICE_DURATION } = require("./constants");

// The panel's window global is an EventEmitter firing the following events:
const EVENTS = {
  // When the accessibility inspector has a new accessible front selected.
  NEW_ACCESSIBLE_FRONT_SELECTED: "Accessibility:NewAccessibleFrontSelected",
  // When the accessibility inspector has a new accessible front highlighted.
  NEW_ACCESSIBLE_FRONT_HIGHLIGHTED: "Accessibility:NewAccessibleFrontHighlighted",
  // When the accessibility inspector has a new accessible front inspected.
  NEW_ACCESSIBLE_FRONT_INSPECTED: "Accessibility:NewAccessibleFrontInspected",
  // When the accessibility inspector is updated.
  ACCESSIBILITY_INSPECTOR_UPDATED: "Accessibility:AccessibilityInspectorUpdated"
};

/**
 * This object represents Accessibility panel. It's responsibility is to
 * render Accessibility Tree of the current debugger target and the sidebar that
 * displays current relevant accessible details.
 */
function AccessibilityPanel(iframeWindow, toolbox) {
  this.panelWin = iframeWindow;
  this._toolbox = toolbox;

  this.onTabNavigated = this.onTabNavigated.bind(this);
  this.onPanelVisibilityChange = this.onPanelVisibilityChange.bind(this);
  this.onNewAccessibleFrontSelected =
    this.onNewAccessibleFrontSelected.bind(this);
  this.onAccessibilityInspectorUpdated =
    this.onAccessibilityInspectorUpdated.bind(this);
  this.updateA11YServiceDurationTimer = this.updateA11YServiceDurationTimer.bind(this);
  this.updatePickerButton = this.updatePickerButton.bind(this);

  EventEmitter.decorate(this);
}

AccessibilityPanel.prototype = {
  /**
   * Open is effectively an asynchronous constructor.
   */
  async open() {
    if (this._opening) {
      await this._opening;
      return this._opening;
    }

    let resolver;
    this._opening = new Promise(resolve => {
      resolver = resolve;
    });

    // Local monitoring needs to make the target remote.
    if (!this.target.isRemote) {
      await this.target.makeRemote();
    }

    this._telemetry = new Telemetry();
    this.panelWin.gTelemetry = this._telemetry;

    this.target.on("navigate", this.onTabNavigated);
    this._toolbox.on("select", this.onPanelVisibilityChange);

    this.panelWin.EVENTS = EVENTS;
    EventEmitter.decorate(this.panelWin);
    this.panelWin.on(EVENTS.NEW_ACCESSIBLE_FRONT_SELECTED,
      this.onNewAccessibleFrontSelected);
    this.panelWin.on(EVENTS.ACCESSIBILITY_INSPECTOR_UPDATED,
      this.onAccessibilityInspectorUpdated);

    this.shouldRefresh = true;
    this.panelWin.gToolbox = this._toolbox;

    await this._toolbox.initInspector();
    this._front = new AccessibilityFront(this.target.client,
                                         this.target.form);
    this._walker = await this._front.getWalker();

    this._isOldVersion = !(await this.target.actorHasMethod("accessibility", "enable"));
    if (!this._isOldVersion) {
      await this._front.bootstrap();
      this.picker = new Picker(this);
    }

    this.updateA11YServiceDurationTimer();
    this._front.on("init", this.updateA11YServiceDurationTimer);
    this._front.on("shutdown", this.updateA11YServiceDurationTimer);

    this.isReady = true;
    this.emit("ready");
    resolver(this);
    return this._opening;
  },

  onNewAccessibleFrontSelected(selected) {
    this.emit("new-accessible-front-selected", selected);
  },

  onAccessibilityInspectorUpdated() {
    this.emit("accessibility-inspector-updated");
  },

  /**
   * Make sure the panel is refreshed when the page is reloaded. The panel is
   * refreshed immediatelly if it's currently selected or lazily when the user
   * actually selects it.
   */
  onTabNavigated() {
    this.shouldRefresh = true;
    this._opening.then(() => this.refresh());
  },

  /**
   * Make sure the panel is refreshed (if needed) when it's selected.
   */
  onPanelVisibilityChange() {
    this._opening.then(() => this.refresh());
  },

  refresh() {
    this.cancelPicker();

    if (this.isVisible) {
      this._front.on("init", this.updatePickerButton);
      this._front.on("shutdown", this.updatePickerButton);
    } else {
      this._front.off("init", this.updatePickerButton);
      this._front.off("shutdown", this.updatePickerButton);
      // Do not refresh if the panel isn't visible.
      return;
    }

    // Do not refresh if it isn't necessary.
    if (!this.shouldRefresh) {
      return;
    }
    // Alright reset the flag we are about to refresh the panel.
    this.shouldRefresh = false;
    this.postContentMessage("initialize", this._front, this._walker, this._isOldVersion);
  },

  updateA11YServiceDurationTimer() {
    if (this._front.enabled) {
      this._telemetry.startTimer(A11Y_SERVICE_DURATION);
    } else {
      this._telemetry.stopTimer(A11Y_SERVICE_DURATION);
    }
  },

  selectAccessible(accessibleFront) {
    this.postContentMessage("selectAccessible", this._walker, accessibleFront);
  },

  selectAccessibleForNode(nodeFront, reason) {
    if (reason) {
      this._telemetry.logKeyedScalar(
        "devtools.accessibility.select_accessible_for_node", reason, 1);
    }

    this.postContentMessage("selectNodeAccessible", this._walker, nodeFront);
  },

  highlightAccessible(accessibleFront) {
    this.postContentMessage("highlightAccessible", this._walker, accessibleFront);
  },

  postContentMessage(type, ...args) {
    const event = new this.panelWin.MessageEvent("devtools/chrome/message", {
      bubbles: true,
      cancelable: true,
      data: { type, args }
    });

    this.panelWin.dispatchEvent(event);
  },

  updatePickerButton() {
    this.picker && this.picker.updateButton();
  },

  togglePicker(focus) {
    this.picker && this.picker.toggle();
  },

  cancelPicker() {
    this.picker && this.picker.cancel();
  },

  stopPicker() {
    this.picker && this.picker.stop();
  },

  get walker() {
    return this._walker;
  },

  /**
   * Return true if the Accessibility panel is currently selected.
   */
  get isVisible() {
    return this._toolbox.currentToolId === "accessibility";
  },

  get target() {
    return this._toolbox.target;
  },

  async destroy() {
    if (this._destroying) {
      await this._destroying;
      return;
    }

    let resolver;
    this._destroying = new Promise(resolve => {
      resolver = resolve;
    });

    this._telemetry.destroy();

    this.target.off("navigate", this.onTabNavigated);
    this._toolbox.off("select", this.onPanelVisibilityChange);

    this.panelWin.off(EVENTS.NEW_ACCESSIBLE_FRONT_SELECTED,
      this.onNewAccessibleFrontSelected);
    this.panelWin.off(EVENTS.ACCESSIBILITY_INSPECTOR_UPDATED,
      this.onAccessibilityInspectorUpdated);

    this.picker.release();
    this.picker = null;

    if (this._front) {
      this._front.off("init", this.updateA11YServiceDurationTimer);
      this._front.off("shutdown", this.updateA11YServiceDurationTimer);
      await this._front.destroy();
    }

    this._front = null;
    this._telemetry = null;
    this.panelWin.gToolbox = null;
    this.panelWin.gTelemetry = null;

    this.emit("destroyed");

    resolver();
  }
};

// Exports from this module
exports.AccessibilityPanel = AccessibilityPanel;
