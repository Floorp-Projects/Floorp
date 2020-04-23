/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Services = require("Services");
const { L10nRegistry } = require("resource://gre/modules/L10nRegistry.jsm");

const EventEmitter = require("devtools/shared/event-emitter");

const Telemetry = require("devtools/client/shared/telemetry");

const { Picker } = require("devtools/client/accessibility/picker");
const {
  A11Y_SERVICE_DURATION,
} = require("devtools/client/accessibility/constants");

// The panel's window global is an EventEmitter firing the following events:
const EVENTS = {
  // When the accessibility inspector has a new accessible front selected.
  NEW_ACCESSIBLE_FRONT_SELECTED: "Accessibility:NewAccessibleFrontSelected",
  // When the accessibility inspector has a new accessible front highlighted.
  NEW_ACCESSIBLE_FRONT_HIGHLIGHTED:
    "Accessibility:NewAccessibleFrontHighlighted",
  // When the accessibility inspector has a new accessible front inspected.
  NEW_ACCESSIBLE_FRONT_INSPECTED: "Accessibility:NewAccessibleFrontInspected",
  // When the accessibility inspector is updated.
  ACCESSIBILITY_INSPECTOR_UPDATED:
    "Accessibility:AccessibilityInspectorUpdated",
};

/**
 * This object represents Accessibility panel. It's responsibility is to
 * render Accessibility Tree of the current debugger target and the sidebar that
 * displays current relevant accessible details.
 */
function AccessibilityPanel(iframeWindow, toolbox, startup) {
  this.panelWin = iframeWindow;
  this._toolbox = toolbox;
  this.startup = startup;

  this.onTabNavigated = this.onTabNavigated.bind(this);
  this.onTargetAvailable = this.onTargetAvailable.bind(this);
  this.onPanelVisibilityChange = this.onPanelVisibilityChange.bind(this);
  this.onNewAccessibleFrontSelected = this.onNewAccessibleFrontSelected.bind(
    this
  );
  this.onAccessibilityInspectorUpdated = this.onAccessibilityInspectorUpdated.bind(
    this
  );
  this.updateA11YServiceDurationTimer = this.updateA11YServiceDurationTimer.bind(
    this
  );
  this.forceUpdatePickerButton = this.forceUpdatePickerButton.bind(this);

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

    this._telemetry = new Telemetry();
    this.panelWin.gTelemetry = this._telemetry;

    this._toolbox.on("select", this.onPanelVisibilityChange);

    this.panelWin.EVENTS = EVENTS;
    EventEmitter.decorate(this.panelWin);
    this.panelWin.on(
      EVENTS.NEW_ACCESSIBLE_FRONT_SELECTED,
      this.onNewAccessibleFrontSelected
    );
    this.panelWin.on(
      EVENTS.ACCESSIBILITY_INSPECTOR_UPDATED,
      this.onAccessibilityInspectorUpdated
    );

    this.shouldRefresh = true;

    await this.startup.initAccessibility();

    await this._toolbox.targetList.watchTargets(
      [this._toolbox.targetList.TYPES.FRAME],
      this.onTargetAvailable
    );

    // Bug 1602075: if auto init feature is enabled, enable accessibility
    // service if necessary.
    if (
      this.accessibilityProxy.supports.autoInit &&
      this.accessibilityProxy.canBeEnabled &&
      !this.accessibilityProxy.enabled
    ) {
      await this.accessibilityProxy.enableAccessibility();
    }

    this.picker = new Picker(this);
    this.fluentBundles = await this.createFluentBundles();

    this.updateA11YServiceDurationTimer();
    this.accessibilityProxy.startListeningForLifecycleEvents({
      init: [this.updateA11YServiceDurationTimer, this.forceUpdatePickerButton],
      shutdown: [
        this.updateA11YServiceDurationTimer,
        this.forceUpdatePickerButton,
      ],
    });

    this.isReady = true;
    this.emit("ready");
    resolver(this);
    return this._opening;
  },

  /**
   * Retrieve message contexts for the current locales, and return them as an
   * array of FluentBundles elements.
   */
  async createFluentBundles() {
    const locales = Services.locale.appLocalesAsBCP47;
    const generator = L10nRegistry.generateBundles(locales, [
      "devtools/client/accessibility.ftl",
    ]);

    // Return value of generateBundles is a generator and should be converted to
    // a sync iterable before using it with React.
    const contexts = [];
    for await (const message of generator) {
      contexts.push(message);
    }

    return contexts;
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

  async onTargetAvailable({ targetFront, isTopLevel, isTargetSwitching }) {
    if (isTopLevel) {
      await this.accessibilityProxy.initializeProxyForPanel(targetFront);
      this.accessibilityProxy.currentTarget.on("navigate", this.onTabNavigated);
    }

    if (isTargetSwitching) {
      this.onTabNavigated();
    }
  },

  /**
   * Make sure the panel is refreshed (if needed) when it's selected.
   */
  onPanelVisibilityChange() {
    this._opening.then(() => this.refresh());
  },

  refresh() {
    this.cancelPicker();

    if (!this.isVisible) {
      // Do not refresh if the panel isn't visible.
      return;
    }

    // Do not refresh if it isn't necessary.
    if (!this.shouldRefresh) {
      return;
    }
    // Alright reset the flag we are about to refresh the panel.
    this.shouldRefresh = false;
    this.postContentMessage("initialize", {
      supports: this.accessibilityProxy.supports,
      fluentBundles: this.fluentBundles,
      toolbox: this._toolbox,
      getAccessibilityTreeRoot: this.accessibilityProxy
        .getAccessibilityTreeRoot,
      startListeningForAccessibilityEvents: this.accessibilityProxy
        .startListeningForAccessibilityEvents,
      stopListeningForAccessibilityEvents: this.accessibilityProxy
        .stopListeningForAccessibilityEvents,
      audit: this.accessibilityProxy.audit,
      simulate: this.accessibilityProxy.simulate,
      enableAccessibility: this.accessibilityProxy.enableAccessibility,
      disableAccessibility: this.accessibilityProxy.disableAccessibility,
      resetAccessiblity: this.accessibilityProxy.resetAccessiblity,
      startListeningForLifecycleEvents: this.accessibilityProxy
        .startListeningForLifecycleEvents,
      stopListeningForLifecycleEvents: this.accessibilityProxy
        .stopListeningForLifecycleEvents,
    });
  },

  updateA11YServiceDurationTimer() {
    if (this.accessibilityProxy.enabled) {
      this._telemetry.start(A11Y_SERVICE_DURATION, this);
    } else {
      this._telemetry.finish(A11Y_SERVICE_DURATION, this, true);
    }
  },

  selectAccessible(accessibleFront) {
    this.postContentMessage("selectAccessible", accessibleFront);
  },

  selectAccessibleForNode(nodeFront, reason) {
    if (reason) {
      this._telemetry.keyedScalarAdd(
        "devtools.accessibility.select_accessible_for_node",
        reason,
        1
      );
    }

    this.postContentMessage("selectNodeAccessible", nodeFront);
  },

  highlightAccessible(accessibleFront) {
    this.postContentMessage("highlightAccessible", accessibleFront);
  },

  postContentMessage(type, ...args) {
    const event = new this.panelWin.MessageEvent("devtools/chrome/message", {
      bubbles: true,
      cancelable: true,
      data: { type, args },
    });

    this.panelWin.dispatchEvent(event);
  },

  updatePickerButton() {
    this.picker && this.picker.updateButton();
  },

  forceUpdatePickerButton() {
    // Only update picker button when the panel is selected.
    if (!this.isVisible) {
      return;
    }

    this.updatePickerButton();
    // Calling setToolboxButtons to make sure toolbar is forced to re-render.
    this._toolbox.component.setToolboxButtons(this._toolbox.toolbarButtons);
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

  get accessibilityProxy() {
    return this.startup.accessibilityProxy;
  },

  /**
   * Return true if the Accessibility panel is currently selected.
   */
  get isVisible() {
    return this._toolbox.currentToolId === "accessibility";
  },

  destroy() {
    if (this._destroyed) {
      return;
    }
    this._destroyed = true;

    this._toolbox.targetList.unwatchTargets(
      [this._toolbox.targetList.TYPES.FRAME],
      this.onTargetAvailable
    );

    this.postContentMessage("destroy");

    this.accessibilityProxy.currentTarget.off("navigate", this.onTabNavigated);
    this._toolbox.off("select", this.onPanelVisibilityChange);

    this.panelWin.off(
      EVENTS.NEW_ACCESSIBLE_FRONT_SELECTED,
      this.onNewAccessibleFrontSelected
    );
    this.panelWin.off(
      EVENTS.ACCESSIBILITY_INSPECTOR_UPDATED,
      this.onAccessibilityInspectorUpdated
    );

    // Older versions of devtools server do not support picker functionality.
    if (this.picker) {
      this.picker.release();
      this.picker = null;
    }

    this.accessibilityProxy.stopListeningForLifecycleEvents({
      init: [this.updateA11YServiceDurationTimer, this.forceUpdatePickerButton],
      shutdown: [
        this.updateA11YServiceDurationTimer,
        this.forceUpdatePickerButton,
      ],
    });

    this._telemetry = null;
    this.panelWin.gTelemetry = null;

    this.emit("destroyed");
  },
};

// Exports from this module
exports.AccessibilityPanel = AccessibilityPanel;
