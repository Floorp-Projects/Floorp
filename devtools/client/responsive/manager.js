/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const promise = require("promise");
const EventEmitter = require("devtools/shared/event-emitter");

loader.lazyRequireGetter(this, "ResponsiveUI", "devtools/client/responsive/ui");
loader.lazyRequireGetter(
  this,
  "startup",
  "devtools/client/responsive/utils/window",
  true
);
loader.lazyRequireGetter(
  this,
  "showNotification",
  "devtools/client/responsive/utils/notification",
  true
);
loader.lazyRequireGetter(this, "l10n", "devtools/client/responsive/utils/l10n");
loader.lazyRequireGetter(
  this,
  "PriorityLevels",
  "devtools/client/shared/components/NotificationBox",
  true
);
loader.lazyRequireGetter(
  this,
  "TargetFactory",
  "devtools/client/framework/target",
  true
);
loader.lazyRequireGetter(
  this,
  "gDevTools",
  "devtools/client/framework/devtools",
  true
);
loader.lazyRequireGetter(this, "Telemetry", "devtools/client/shared/telemetry");

/**
 * ResponsiveUIManager is the external API for the browser UI, etc. to use when
 * opening and closing the responsive UI.
 */
const ResponsiveUIManager = (exports.ResponsiveUIManager = {
  _telemetry: new Telemetry(),

  activeTabs: new Map(),

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
   *          - `menu`:     Web Developer menu item
   *          - `shortcut`: Keyboard shortcut
   * @return Promise
   *         Resolved when the toggling has completed.  If the UI has opened,
   *         it is resolved to the ResponsiveUI instance for this tab.  If the
   *         the UI has closed, there is no resolution value.
   */
  toggle(window, tab, options = {}) {
    const action = this.isActiveForTab(tab) ? "close" : "open";
    const completed = this[action + "IfNeeded"](window, tab, options);
    completed.catch(console.error);
    return completed;
  },

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
   *          - `menu`:     Web Developer menu item
   *          - `shortcut`: Keyboard shortcut
   * @return Promise
   *         Resolved to the ResponsiveUI instance for this tab when opening is
   *         complete.
   */
  async openIfNeeded(window, tab, options = {}) {
    if (!tab.linkedBrowser.isRemoteBrowser) {
      await this.showRemoteOnlyNotification(window, tab, options);
      return promise.reject(new Error("RDM only available for remote tabs."));
    }
    if (!this.isActiveForTab(tab)) {
      this.initMenuCheckListenerFor(window);

      const ui = new ResponsiveUI(this, window, tab);
      this.activeTabs.set(tab, ui);

      // Explicitly not await on telemetry to avoid delaying RDM opening
      this.recordTelemetryOpen(window, tab, options);

      await this.setMenuCheckFor(tab, window);
      await ui.inited;
      this.emit("on", { tab });
    }

    return this.getResponsiveUIForTab(tab);
  },

  /**
   * Record all telemetry probes related to RDM opening.
   */
  async recordTelemetryOpen(window, tab, options) {
    // Track whether a toolbox was opened before RDM was opened.
    const isKnownTab = TargetFactory.isKnownTab(tab);
    let toolbox;
    if (isKnownTab) {
      const target = await TargetFactory.forTab(tab);
      toolbox = gDevTools.getToolbox(target);
    }
    const hostType = toolbox ? toolbox.hostType : "none";
    const hasToolbox = !!toolbox;
    const tel = this._telemetry;
    if (hasToolbox) {
      tel.scalarAdd("devtools.responsive.toolbox_opened_first", 1);
    }

    tel.recordEvent("activate", "responsive_design", null, {
      host: hostType,
      width: Math.ceil(window.outerWidth / 50) * 50,
      session_id: toolbox ? toolbox.sessionId : -1,
    });

    // Track opens keyed by the UI entry point used.
    let { trigger } = options;
    if (!trigger) {
      trigger = "unknown";
    }
    tel.keyedScalarAdd("devtools.responsive.open_trigger", trigger, 1);
  },

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
   *          - `menu`:     Web Developer menu item
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
  },

  async recordTelemetryClose(window, tab) {
    const isKnownTab = TargetFactory.isKnownTab(tab);
    let toolbox;
    if (isKnownTab) {
      const target = await TargetFactory.forTab(tab);
      toolbox = gDevTools.getToolbox(target);
    }

    const hostType = toolbox ? toolbox.hostType : "none";
    const t = this._telemetry;
    t.recordEvent("deactivate", "responsive_design", null, {
      host: hostType,
      width: Math.ceil(window.outerWidth / 50) * 50,
      session_id: toolbox ? toolbox.sessionId : -1,
    });
  },

  /**
   * Returns true if responsive UI is active for a given tab.
   *
   * @param tab
   *        The browser tab.
   * @return boolean
   */
  isActiveForTab(tab) {
    return this.activeTabs.has(tab);
  },

  /**
   * Returns true if responsive UI is active in any tab in the given window.
   *
   * @param window
   *        The main browser chrome window.
   * @return boolean
   */
  isActiveForWindow(window) {
    return [...this.activeTabs.keys()].some(t => t.ownerGlobal === window);
  },

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
  },

  handleMenuCheck({ target }) {
    ResponsiveUIManager.setMenuCheckFor(target);
  },

  initMenuCheckListenerFor(window) {
    const { tabContainer } = window.gBrowser;
    tabContainer.addEventListener("TabSelect", this.handleMenuCheck);
  },

  removeMenuCheckListenerFor(window) {
    if (window && window.gBrowser && window.gBrowser.tabContainer) {
      const { tabContainer } = window.gBrowser;
      tabContainer.removeEventListener("TabSelect", this.handleMenuCheck);
    }
  },

  async setMenuCheckFor(tab, window = tab.ownerGlobal) {
    await startup(window);

    const menu = window.document.getElementById("menu_responsiveUI");
    if (menu) {
      menu.setAttribute("checked", this.isActiveForTab(tab));
    }
  },

  showRemoteOnlyNotification(window, tab, { trigger } = {}) {
    return showNotification(window, tab, {
      toolboxButton: trigger == "toolbox",
      msg: l10n.getStr("responsive.remoteOnly"),
      priority: PriorityLevels.PRIORITY_CRITICAL_MEDIUM,
    });
  },
});

EventEmitter.decorate(ResponsiveUIManager);
