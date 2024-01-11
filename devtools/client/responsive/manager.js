/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("resource://devtools/shared/event-emitter.js");

loader.lazyRequireGetter(
  this,
  "ResponsiveUI",
  "resource://devtools/client/responsive/ui.js"
);
loader.lazyRequireGetter(
  this,
  "startup",
  "resource://devtools/client/responsive/utils/window.js",
  true
);
loader.lazyRequireGetter(
  this,
  "showNotification",
  "resource://devtools/client/responsive/utils/notification.js",
  true
);
loader.lazyRequireGetter(
  this,
  "l10n",
  "resource://devtools/client/responsive/utils/l10n.js"
);
loader.lazyRequireGetter(
  this,
  "PriorityLevels",
  "resource://devtools/client/shared/components/NotificationBox.js",
  true
);
loader.lazyRequireGetter(
  this,
  "gDevTools",
  "resource://devtools/client/framework/devtools.js",
  true
);
loader.lazyRequireGetter(
  this,
  "gDevToolsBrowser",
  "resource://devtools/client/framework/devtools-browser.js",
  true
);
loader.lazyRequireGetter(
  this,
  "Telemetry",
  "resource://devtools/client/shared/telemetry.js"
);

/**
 * ResponsiveUIManager is the external API for the browser UI, etc. to use when
 * opening and closing the responsive UI.
 */
class ResponsiveUIManager {
  constructor() {
    this.activeTabs = new Map();

    this.handleMenuCheck = this.handleMenuCheck.bind(this);

    EventEmitter.decorate(this);
  }

  get telemetry() {
    if (!this._telemetry) {
      this._telemetry = new Telemetry();
    }

    return this._telemetry;
  }

  /**
   * Toggle the responsive UI for a tab.
   *
   * @param window
   *        The main browser chrome window.
   * @param tab
   *        The browser tab.
   * @param options
   *        Other options associated with toggling.  Currently includes:
   *        - `trigger`: String denoting the UI entry point, such as:
   *          - `toolbox`:  Toolbox Button
   *          - `menu`:     Browser Tools menu item
   *          - `shortcut`: Keyboard shortcut
   * @return Promise
   *         Resolved when the toggling has completed.  If the UI has opened,
   *         it is resolved to the ResponsiveUI instance for this tab.  If the
   *         the UI has closed, there is no resolution value.
   */
  toggle(window, tab, options = {}) {
    const completed = this._toggleForTab(window, tab, options);
    completed.catch(console.error);
    return completed;
  }

  _toggleForTab(window, tab, options) {
    if (this.isActiveForTab(tab)) {
      return this.closeIfNeeded(window, tab, options);
    }

    return this.openIfNeeded(window, tab, options);
  }

  /**
   * Opens the responsive UI, if not already open.
   *
   * @param window
   *        The main browser chrome window.
   * @param tab
   *        The browser tab.
   * @param options
   *        Other options associated with opening.  Currently includes:
   *        - `trigger`: String denoting the UI entry point, such as:
   *          - `toolbox`:  Toolbox Button
   *          - `menu`:     Browser Tools menu item
   *          - `shortcut`: Keyboard shortcut
   * @return Promise
   *         Resolved to the ResponsiveUI instance for this tab when opening is
   *         complete.
   */
  async openIfNeeded(window, tab, options = {}) {
    if (!this.isActiveForTab(tab)) {
      this.initMenuCheckListenerFor(window);

      const ui = new ResponsiveUI(this, window, tab);
      this.activeTabs.set(tab, ui);

      // Explicitly not await on telemetry to avoid delaying RDM opening
      this.recordTelemetryOpen(window, tab, options);

      await gDevToolsBrowser.loadBrowserStyleSheet(window);
      await this.setMenuCheckFor(tab, window);
      await ui.inited;
      this.emit("on", { tab });
    }

    return this.getResponsiveUIForTab(tab);
  }

  /**
   * Record all telemetry probes related to RDM opening.
   */
  recordTelemetryOpen(window, tab, options) {
    // Track whether a toolbox was opened before RDM was opened.
    const toolbox = gDevTools.getToolboxForTab(tab);
    const hostType = toolbox ? toolbox.hostType : "none";
    const hasToolbox = !!toolbox;

    if (hasToolbox) {
      this.telemetry.scalarAdd("devtools.responsive.toolbox_opened_first", 1);
    }

    this.telemetry.recordEvent("activate", "responsive_design", null, {
      host: hostType,
      width: Math.ceil(window.outerWidth / 50) * 50,
    });

    // Track opens keyed by the UI entry point used.
    let { trigger } = options;
    if (!trigger) {
      trigger = "unknown";
    }
    this.telemetry.keyedScalarAdd(
      "devtools.responsive.open_trigger",
      trigger,
      1
    );
  }

  /**
   * Closes the responsive UI, if not already closed.
   *
   * @param window
   *        The main browser chrome window.
   * @param tab
   *        The browser tab.
   * @param options
   *        Other options associated with closing.  Currently includes:
   *        - `trigger`: String denoting the UI entry point, such as:
   *          - `toolbox`:  Toolbox Button
   *          - `menu`:     Browser Tools menu item
   *          - `shortcut`: Keyboard shortcut
   *        - `reason`: String detailing the specific cause for closing
   * @return Promise
   *         Resolved (with no value) when closing is complete.
   */
  async closeIfNeeded(window, tab, options = {}) {
    if (this.isActiveForTab(tab)) {
      const ui = this.activeTabs.get(tab);
      const destroyed = await ui.destroy(options);
      if (!destroyed) {
        // Already in the process of destroying, abort.
        return;
      }

      this.activeTabs.delete(tab);

      if (!this.isActiveForWindow(window)) {
        this.removeMenuCheckListenerFor(window);
      }
      this.emit("off", { tab });
      await this.setMenuCheckFor(tab, window);

      // Explicitly not await on telemetry to avoid delaying RDM closing
      this.recordTelemetryClose(window, tab);
    }
  }

  recordTelemetryClose(window, tab) {
    const toolbox = gDevTools.getToolboxForTab(tab);

    const hostType = toolbox ? toolbox.hostType : "none";

    this.telemetry.recordEvent("deactivate", "responsive_design", null, {
      host: hostType,
      width: Math.ceil(window.outerWidth / 50) * 50,
    });
  }

  /**
   * Returns true if responsive UI is active for a given tab.
   *
   * @param tab
   *        The browser tab.
   * @return boolean
   */
  isActiveForTab(tab) {
    return this.activeTabs.has(tab);
  }

  /**
   * Returns true if responsive UI is active in any tab in the given window.
   *
   * @param window
   *        The main browser chrome window.
   * @return boolean
   */
  isActiveForWindow(window) {
    return [...this.activeTabs.keys()].some(t => t.ownerGlobal === window);
  }

  /**
   * Return the responsive UI controller for a tab.
   *
   * @param tab
   *        The browser tab.
   * @return ResponsiveUI
   *         The UI instance for this tab.
   */
  getResponsiveUIForTab(tab) {
    return this.activeTabs.get(tab);
  }

  handleMenuCheck({ target }) {
    this.setMenuCheckFor(target);
  }

  initMenuCheckListenerFor(window) {
    const { tabContainer } = window.gBrowser;
    tabContainer.addEventListener("TabSelect", this.handleMenuCheck);
  }

  removeMenuCheckListenerFor(window) {
    if (window?.gBrowser?.tabContainer) {
      const { tabContainer } = window.gBrowser;
      tabContainer.removeEventListener("TabSelect", this.handleMenuCheck);
    }
  }

  async setMenuCheckFor(tab, window = tab.ownerGlobal) {
    await startup(window);

    const menu = window.document.getElementById("menu_responsiveUI");
    if (menu) {
      menu.setAttribute("checked", this.isActiveForTab(tab));
    }
  }

  showRemoteOnlyNotification(window, tab, { trigger } = {}) {
    return showNotification(window, tab, {
      toolboxButton: trigger == "toolbox",
      msg: l10n.getStr("responsive.remoteOnly"),
      priority: PriorityLevels.PRIORITY_CRITICAL_MEDIUM,
    });
  }
}

module.exports = new ResponsiveUIManager();
