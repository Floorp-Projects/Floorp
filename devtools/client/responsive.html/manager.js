/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const promise = require("promise");
const Services = require("Services");
const EventEmitter = require("devtools/shared/event-emitter");

const TOOL_URL = "chrome://devtools/content/responsive.html/index.xhtml";

loader.lazyRequireGetter(this, "DebuggerClient", "devtools/shared/client/debugger-client", true);
loader.lazyRequireGetter(this, "DebuggerServer", "devtools/server/main", true);
loader.lazyRequireGetter(this, "throttlingProfiles", "devtools/client/shared/components/throttling/profiles");
loader.lazyRequireGetter(this, "swapToInnerBrowser", "devtools/client/responsive.html/browser/swap", true);
loader.lazyRequireGetter(this, "startup", "devtools/client/responsive.html/utils/window", true);
loader.lazyRequireGetter(this, "message", "devtools/client/responsive.html/utils/message");
loader.lazyRequireGetter(this, "showNotification", "devtools/client/responsive.html/utils/notification", true);
loader.lazyRequireGetter(this, "l10n", "devtools/client/responsive.html/utils/l10n");
loader.lazyRequireGetter(this, "EmulationFront", "devtools/shared/fronts/emulation", true);
loader.lazyRequireGetter(this, "PriorityLevels", "devtools/client/shared/components/NotificationBox", true);
loader.lazyRequireGetter(this, "TargetFactory", "devtools/client/framework/target", true);
loader.lazyRequireGetter(this, "gDevTools", "devtools/client/framework/devtools", true);
loader.lazyRequireGetter(this, "Telemetry", "devtools/client/shared/telemetry");

const RELOAD_CONDITION_PREF_PREFIX = "devtools.responsive.reloadConditions.";
const RELOAD_NOTIFICATION_PREF = "devtools.responsive.reloadNotification.enabled";

function debug(msg) {
  // console.log(`RDM manager: ${msg}`);
}

/**
 * ResponsiveUIManager is the external API for the browser UI, etc. to use when
 * opening and closing the responsive UI.
 */
const ResponsiveUIManager = exports.ResponsiveUIManager = {
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
   *          - `command`:  GCLI command bar or toolbox button
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
   *          - `command`:  GCLI command bar or toolbox button
   *          - `menu`:     Web Developer menu item
   *          - `shortcut`: Keyboard shortcut
   * @return Promise
   *         Resolved to the ResponsiveUI instance for this tab when opening is
   *         complete.
   */
  async openIfNeeded(window, tab, options = {}) {
    if (!tab.linkedBrowser.isRemoteBrowser) {
      this.showRemoteOnlyNotification(window, tab, options);
      return promise.reject(new Error("RDM only available for remote tabs."));
    }
    if (!this.isActiveForTab(tab)) {
      this.initMenuCheckListenerFor(window);

      // Track whether a toolbox was opened before RDM was opened.
      const toolbox = gDevTools.getToolbox(TargetFactory.forTab(tab));
      const hostType = toolbox ? toolbox.hostType : "none";
      const hasToolbox = !!toolbox;
      const tel = this._telemetry;
      if (hasToolbox) {
        tel.scalarAdd("devtools.responsive.toolbox_opened_first", 1);
      }

      tel.recordEvent("devtools.main", "activate", "responsive_design", null, {
        "host": hostType,
        "width": Math.ceil(window.outerWidth / 50) * 50,
        "session_id": toolbox ? toolbox.sessionId : -1
      });

      // Track opens keyed by the UI entry point used.
      let { trigger } = options;
      if (!trigger) {
        trigger = "unknown";
      }
      tel.keyedScalarAdd("devtools.responsive.open_trigger", trigger, 1);

      const ui = new ResponsiveUI(window, tab);
      this.activeTabs.set(tab, ui);
      await this.setMenuCheckFor(tab, window);
      await ui.inited;
      this.emit("on", { tab });
    }

    return this.getResponsiveUIForTab(tab);
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
   *          - `command`:  GCLI command bar or toolbox button
   *          - `menu`:     Web Developer menu item
   *          - `shortcut`: Keyboard shortcut
   *        - `reason`: String detailing the specific cause for closing
   * @return Promise
   *         Resolved (with no value) when closing is complete.
   */
  async closeIfNeeded(window, tab, options = {}) {
    if (this.isActiveForTab(tab)) {
      const target = TargetFactory.forTab(tab);
      const toolbox = gDevTools.getToolbox(target);

      if (!toolbox) {
        // Destroy the tabTarget to avoid a memory leak.
        target.destroy();
      }

      const ui = this.activeTabs.get(tab);
      const destroyed = await ui.destroy(options);
      if (!destroyed) {
        // Already in the process of destroying, abort.
        return;
      }

      const hostType = toolbox ? toolbox.hostType : "none";
      const t = this._telemetry;
      t.recordEvent("devtools.main", "deactivate", "responsive_design", null, {
        "host": hostType,
        "width": Math.ceil(window.outerWidth / 50) * 50,
        "session_id": toolbox ? toolbox.sessionId : -1
      });

      this.activeTabs.delete(tab);

      if (!this.isActiveForWindow(window)) {
        this.removeMenuCheckListenerFor(window);
      }
      this.emit("off", { tab });
      await this.setMenuCheckFor(tab, window);
    }
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

  /**
   * Handle GCLI commands.
   *
   * @param window
   *        The main browser chrome window.
   * @param tab
   *        The browser tab.
   * @param command
   *        The GCLI command name.
   * @param args
   *        The GCLI command arguments.
   */
  handleGcliCommand(window, tab, command, args) {
    let completed;
    switch (command) {
      case "resize to":
        completed = this.openIfNeeded(window, tab, { trigger: "command" });
        this.activeTabs.get(tab).setViewportSize(args);
        break;
      case "resize on":
        completed = this.openIfNeeded(window, tab, { trigger: "command" });
        break;
      case "resize off":
        completed = this.closeIfNeeded(window, tab, { trigger: "command" });
        break;
      case "resize toggle":
        completed = this.toggle(window, tab, { trigger: "command" });
        break;
      default:
    }
    completed.catch(console.error);
  },

  handleMenuCheck({target}) {
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
    showNotification(window, tab, {
      command: trigger == "command",
      msg: l10n.getStr("responsive.remoteOnly"),
      priority: PriorityLevels.PRIORITY_CRITICAL_MEDIUM,
    });
  },
};

// GCLI commands in ./commands.js listen for events from this object to know
// when the UI for a tab has opened or closed.
EventEmitter.decorate(ResponsiveUIManager);

/**
 * ResponsiveUI manages the responsive design tool for a specific tab.  The
 * actual tool itself lives in a separate chrome:// document that is loaded into
 * the tab upon opening responsive design.  This object acts a helper to
 * integrate the tool into the surrounding browser UI as needed.
 */
function ResponsiveUI(window, tab) {
  this.browserWindow = window;
  this.tab = tab;
  this.inited = this.init();
}

ResponsiveUI.prototype = {

  /**
   * The main browser chrome window (that holds many tabs).
   */
  browserWindow: null,

  /**
   * The specific browser tab this responsive instance is for.
   */
  tab: null,

  /**
   * Promise resovled when the UI init has completed.
   */
  inited: null,

  /**
   * Flag set when destruction has begun.
   */
  destroying: false,

  /**
   * Flag set when destruction has ended.
   */
  destroyed: false,

  /**
   * A window reference for the chrome:// document that displays the responsive
   * design tool.  It is safe to reference this window directly even with e10s,
   * as the tool UI is always loaded in the parent process.  The web content
   * contained *within* the tool UI on the other hand is loaded in the child
   * process.
   */
  toolWindow: null,

  /**
   * Open RDM while preserving the state of the page.  We use `swapFrameLoaders`
   * to ensure all in-page state is preserved, just like when you move a tab to
   * a new window.
   *
   * For more details, see /devtools/docs/responsive-design-mode.md.
   */
  async init() {
    debug("Init start");

    const ui = this;

    // Watch for tab close and window close so we can clean up RDM synchronously
    this.tab.addEventListener("TabClose", this);
    this.browserWindow.addEventListener("unload", this);

    // Swap page content from the current tab into a viewport within RDM
    debug("Create browser swapper");
    this.swap = swapToInnerBrowser({
      tab: this.tab,
      containerURL: TOOL_URL,
      async getInnerBrowser(containerBrowser) {
        const toolWindow = ui.toolWindow = containerBrowser.contentWindow;
        toolWindow.addEventListener("message", ui);
        debug("Wait until init from inner");
        await message.request(toolWindow, "init");
        toolWindow.addInitialViewport({
          uri: "about:blank",
          userContextId: ui.tab.userContextId,
        });
        debug("Wait until browser mounted");
        await message.wait(toolWindow, "browser-mounted");
        return ui.getViewportBrowser();
      }
    });
    debug("Wait until swap start");
    await this.swap.start();

    this.tab.addEventListener("BeforeTabRemotenessChange", this);

    // Notify the inner browser to start the frame script
    debug("Wait until start frame script");
    await message.request(this.toolWindow, "start-frame-script");

    // Get the protocol ready to speak with emulation actor
    debug("Wait until RDP server connect");
    await this.connectToServer();

    // Non-blocking message to tool UI to start any delayed init activities
    message.post(this.toolWindow, "post-init");

    debug("Init done");
  },

  /**
   * Close RDM and restore page content back into a regular tab.
   *
   * @param object
   *        Destroy options, which currently includes a `reason` string.
   * @return boolean
   *         Whether this call is actually destroying.  False means destruction
   *         was already in progress.
   */
  async destroy(options) {
    if (this.destroying) {
      return false;
    }
    this.destroying = true;

    // If our tab is about to be closed, there's not enough time to exit
    // gracefully, but that shouldn't be a problem since the tab will go away.
    // So, skip any waiting when we're about to close the tab.
    const isWindowClosing = options && options.reason === "unload";
    const isTabContentDestroying =
      isWindowClosing || (options && (options.reason === "TabClose" ||
                                      options.reason === "BeforeTabRemotenessChange"));

    // Ensure init has finished before starting destroy
    if (!isTabContentDestroying) {
      await this.inited;
    }

    this.tab.removeEventListener("TabClose", this);
    this.tab.removeEventListener("BeforeTabRemotenessChange", this);
    this.browserWindow.removeEventListener("unload", this);
    this.toolWindow.removeEventListener("message", this);

    if (!isTabContentDestroying) {
      // Notify the inner browser to stop the frame script
      await message.request(this.toolWindow, "stop-frame-script");
    }

    // Ensure the tab is reloaded if required when exiting RDM so that no emulated
    // settings are left in a customized state.
    if (!isTabContentDestroying) {
      let reloadNeeded = false;
      await this.updateDPPX();
      await this.updateNetworkThrottling();
      reloadNeeded |= await this.updateUserAgent() &&
                      this.reloadOnChange("userAgent");
      reloadNeeded |= await this.updateTouchSimulation() &&
                      this.reloadOnChange("touchSimulation");
      if (reloadNeeded) {
        this.getViewportBrowser().reload();
      }
    }

    // Destroy local state
    const swap = this.swap;
    this.browserWindow = null;
    this.tab = null;
    this.inited = null;
    this.toolWindow = null;
    this.swap = null;

    // Close the debugger client used to speak with emulation actor.
    // The actor handles clearing any overrides itself, so it's not necessary to clear
    // anything on shutdown client side.
    const clientClosed = this.client.close();
    if (!isTabContentDestroying) {
      await clientClosed;
    }
    this.client = this.emulationFront = null;

    if (!isWindowClosing) {
      // Undo the swap and return the content back to a normal tab
      swap.stop();
    }

    this.destroyed = true;

    return true;
  },

  async connectToServer() {
    DebuggerServer.init();
    DebuggerServer.registerAllActors();
    this.client = new DebuggerClient(DebuggerServer.connectPipe());
    await this.client.connect();
    const { tab } = await this.client.getTab();
    this.emulationFront = EmulationFront(this.client, tab);
  },

  /**
   * Show one-time notification about reloads for emulation.
   */
  showReloadNotification() {
    if (Services.prefs.getBoolPref(RELOAD_NOTIFICATION_PREF, false)) {
      showNotification(this.browserWindow, this.tab, {
        msg: l10n.getFormatStr("responsive.reloadNotification.description",
                               l10n.getStr("responsive.reloadConditions.label")),
      });
      Services.prefs.setBoolPref(RELOAD_NOTIFICATION_PREF, false);
    }
  },

  reloadOnChange(id) {
    this.showReloadNotification();
    const pref = RELOAD_CONDITION_PREF_PREFIX + id;
    return Services.prefs.getBoolPref(pref, false);
  },

  handleEvent(event) {
    const { browserWindow, tab } = this;

    switch (event.type) {
      case "message":
        this.handleMessage(event);
        break;
      case "BeforeTabRemotenessChange":
      case "TabClose":
      case "unload":
        ResponsiveUIManager.closeIfNeeded(browserWindow, tab, {
          reason: event.type,
        });
        break;
    }
  },

  handleMessage(event) {
    if (event.origin !== "chrome://devtools") {
      return;
    }

    switch (event.data.type) {
      case "change-device":
        this.onChangeDevice(event);
        break;
      case "change-network-throttling":
        this.onChangeNetworkThrottling(event);
        break;
      case "change-pixel-ratio":
        this.onChangePixelRatio(event);
        break;
      case "change-touch-simulation":
        this.onChangeTouchSimulation(event);
        break;
      case "content-resize":
        this.onContentResize(event);
        break;
      case "exit":
        this.onExit();
        break;
      case "remove-device-association":
        this.onRemoveDeviceAssociation(event);
        break;
    }
  },

  async onChangeDevice(event) {
    const { userAgent, pixelRatio, touch } = event.data.device;
    let reloadNeeded = false;
    await this.updateDPPX(pixelRatio);
    reloadNeeded |= await this.updateUserAgent(userAgent) &&
                    this.reloadOnChange("userAgent");
    reloadNeeded |= await this.updateTouchSimulation(touch) &&
                    this.reloadOnChange("touchSimulation");
    if (reloadNeeded) {
      this.getViewportBrowser().reload();
    }
    // Used by tests
    this.emit("device-changed");
  },

  async onChangeNetworkThrottling(event) {
    const { enabled, profile } = event.data;
    await this.updateNetworkThrottling(enabled, profile);
    // Used by tests
    this.emit("network-throttling-changed");
  },

  onChangePixelRatio(event) {
    const { pixelRatio } = event.data;
    this.updateDPPX(pixelRatio);
  },

  async onChangeTouchSimulation(event) {
    const { enabled } = event.data;
    const reloadNeeded = await this.updateTouchSimulation(enabled) &&
                       this.reloadOnChange("touchSimulation");
    if (reloadNeeded) {
      this.getViewportBrowser().reload();
    }
    // Used by tests
    this.emit("touch-simulation-changed");
  },

  onContentResize(event) {
    const { width, height } = event.data;
    this.emit("content-resize", {
      width,
      height,
    });
  },

  onExit() {
    const { browserWindow, tab } = this;
    ResponsiveUIManager.closeIfNeeded(browserWindow, tab);
  },

  async onRemoveDeviceAssociation(event) {
    let reloadNeeded = false;
    await this.updateDPPX();
    reloadNeeded |= await this.updateUserAgent() &&
                    this.reloadOnChange("userAgent");
    reloadNeeded |= await this.updateTouchSimulation() &&
                    this.reloadOnChange("touchSimulation");
    if (reloadNeeded) {
      this.getViewportBrowser().reload();
    }
    // Used by tests
    this.emit("device-association-removed");
  },

  /**
   * Set or clear the emulated device pixel ratio.
   *
   * @return boolean
   *         Whether a reload is needed to apply the change.
   *         (This is always immediate, so it's always false.)
   */
  async updateDPPX(dppx) {
    if (!dppx) {
      await this.emulationFront.clearDPPXOverride();
      return false;
    }
    await this.emulationFront.setDPPXOverride(dppx);
    return false;
  },

  /**
   * Set or clear network throttling.
   *
   * @return boolean
   *         Whether a reload is needed to apply the change.
   *         (This is always immediate, so it's always false.)
   */
  async updateNetworkThrottling(enabled, profile) {
    if (!enabled) {
      await this.emulationFront.clearNetworkThrottling();
      return false;
    }
    const data = throttlingProfiles.find(({ id }) => id == profile);
    const { download, upload, latency } = data;
    await this.emulationFront.setNetworkThrottling({
      downloadThroughput: download,
      uploadThroughput: upload,
      latency,
    });
    return false;
  },

  /**
   * Set or clear the emulated user agent.
   *
   * @return boolean
   *         Whether a reload is needed to apply the change.
   */
  updateUserAgent(userAgent) {
    if (!userAgent) {
      return this.emulationFront.clearUserAgentOverride();
    }
    return this.emulationFront.setUserAgentOverride(userAgent);
  },

  /**
   * Set or clear touch simulation.
   *
   * @return boolean
   *         Whether a reload is needed to apply the change.
   */
  updateTouchSimulation(enabled) {
    if (!enabled) {
      return this.emulationFront.clearTouchEventsOverride();
    }
    return this.emulationFront.setTouchEventsOverride(
      Ci.nsIDocShell.TOUCHEVENTS_OVERRIDE_ENABLED
    );
  },

  /**
   * Helper for tests. Assumes a single viewport for now.
   */
  getViewportSize() {
    return this.toolWindow.getViewportSize();
  },

  /**
   * Helper for tests, GCLI, etc. Assumes a single viewport for now.
   */
  async setViewportSize(size) {
    await this.inited;
    this.toolWindow.setViewportSize(size);
  },

  /**
   * Helper for tests/reloading the viewport. Assumes a single viewport for now.
   */
  getViewportBrowser() {
    return this.toolWindow.getViewportBrowser();
  },

  /**
   * Helper for contacting the viewport content. Assumes a single viewport for now.
   */
  getViewportMessageManager() {
    return this.getViewportBrowser().messageManager;
  },

};

EventEmitter.decorate(ResponsiveUI.prototype);
