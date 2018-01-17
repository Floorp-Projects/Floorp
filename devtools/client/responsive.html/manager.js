/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const promise = require("promise");
const EventEmitter = require("devtools/shared/old-event-emitter");

const TOOL_URL = "chrome://devtools/content/responsive.html/index.xhtml";

loader.lazyRequireGetter(this, "DebuggerClient", "devtools/shared/client/debugger-client", true);
loader.lazyRequireGetter(this, "DebuggerServer", "devtools/server/main", true);
loader.lazyRequireGetter(this, "TargetFactory", "devtools/client/framework/target", true);
loader.lazyRequireGetter(this, "gDevTools", "devtools/client/framework/devtools", true);
loader.lazyRequireGetter(this, "throttlingProfiles",
  "devtools/client/shared/network-throttling-profiles");
loader.lazyRequireGetter(this, "swapToInnerBrowser",
  "devtools/client/responsive.html/browser/swap", true);
loader.lazyRequireGetter(this, "startup",
  "devtools/client/responsive.html/utils/window", true);
loader.lazyRequireGetter(this, "message",
  "devtools/client/responsive.html/utils/message");
loader.lazyRequireGetter(this, "getStr",
  "devtools/client/responsive.html/utils/l10n", true);
loader.lazyRequireGetter(this, "EmulationFront",
  "devtools/shared/fronts/emulation", true);

function debug(msg) {
  // console.log(`RDM manager: ${msg}`);
}

/**
 * ResponsiveUIManager is the external API for the browser UI, etc. to use when
 * opening and closing the responsive UI.
 */
const ResponsiveUIManager = exports.ResponsiveUIManager = {
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
   *        - `command`: Whether initiated via GCLI command bar or toolbox button
   * @return Promise
   *         Resolved when the toggling has completed.  If the UI has opened,
   *         it is resolved to the ResponsiveUI instance for this tab.  If the
   *         the UI has closed, there is no resolution value.
   */
  toggle(window, tab, options) {
    let action = this.isActiveForTab(tab) ? "close" : "open";
    let completed = this[action + "IfNeeded"](window, tab, options);
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
   *        - `command`: Whether initiated via GCLI command bar or toolbox button
   * @return Promise
   *         Resolved to the ResponsiveUI instance for this tab when opening is
   *         complete.
   */
  async openIfNeeded(window, tab, options) {
    if (!tab.linkedBrowser.isRemoteBrowser) {
      this.showRemoteOnlyNotification(window, tab, options);
      return promise.reject(new Error("RDM only available for remote tabs."));
    }
    // Remove this once we support this case in bug 1306975.
    if (tab.linkedBrowser.hasAttribute("usercontextid")) {
      this.showNoContainerTabsNotification(window, tab, options);
      return promise.reject(new Error("RDM not available for container tabs."));
    }
    if (!this.isActiveForTab(tab)) {
      this.initMenuCheckListenerFor(window);

      let ui = new ResponsiveUI(window, tab);
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
   *        - `command`: Whether initiated via GCLI command bar or toolbox button
   *        - `reason`: String detailing the specific cause for closing
   * @return Promise
   *         Resolved (with no value) when closing is complete.
   */
  async closeIfNeeded(window, tab, options) {
    if (this.isActiveForTab(tab)) {
      let ui = this.activeTabs.get(tab);
      let destroyed = await ui.destroy(options);
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
        completed = this.openIfNeeded(window, tab, { command: true });
        this.activeTabs.get(tab).setViewportSize(args);
        break;
      case "resize on":
        completed = this.openIfNeeded(window, tab, { command: true });
        break;
      case "resize off":
        completed = this.closeIfNeeded(window, tab, { command: true });
        break;
      case "resize toggle":
        completed = this.toggle(window, tab, { command: true });
        break;
      default:
    }
    completed.catch(console.error);
  },

  handleMenuCheck({target}) {
    ResponsiveUIManager.setMenuCheckFor(target);
  },

  initMenuCheckListenerFor(window) {
    let { tabContainer } = window.gBrowser;
    tabContainer.addEventListener("TabSelect", this.handleMenuCheck);
  },

  removeMenuCheckListenerFor(window) {
    if (window && window.gBrowser && window.gBrowser.tabContainer) {
      let { tabContainer } = window.gBrowser;
      tabContainer.removeEventListener("TabSelect", this.handleMenuCheck);
    }
  },

  async setMenuCheckFor(tab, window = tab.ownerGlobal) {
    await startup(window);

    let menu = window.document.getElementById("menu_responsiveUI");
    if (menu) {
      menu.setAttribute("checked", this.isActiveForTab(tab));
    }
  },

  showRemoteOnlyNotification(window, tab, options) {
    this.showErrorNotification(window, tab, options, getStr("responsive.remoteOnly"));
  },

  showNoContainerTabsNotification(window, tab, options) {
    this.showErrorNotification(window, tab, options,
                               getStr("responsive.noContainerTabs"));
  },

  showErrorNotification(window, tab, { command } = {}, msg) {
    // Default to using the browser's per-tab notification box
    let nbox = window.gBrowser.getNotificationBox(tab.linkedBrowser);

    // If opening was initiated by GCLI command bar or toolbox button, check for an open
    // toolbox for the tab.  If one exists, use the toolbox's notification box so that the
    // message is placed closer to the action taken by the user.
    if (command) {
      let target = TargetFactory.forTab(tab);
      let toolbox = gDevTools.getToolbox(target);
      if (toolbox) {
        nbox = toolbox.notificationBox;
      }
    }

    let value = "devtools-responsive-error";
    if (nbox.getNotificationWithValue(value)) {
      // Notification already displayed
      return;
    }

    nbox.appendNotification(
       msg,
       value,
       null,
       nbox.PRIORITY_CRITICAL_MEDIUM,
       []);
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

    let ui = this;

    // Watch for tab close and window close so we can clean up RDM synchronously
    this.tab.addEventListener("TabClose", this);
    this.browserWindow.addEventListener("unload", this);

    // Swap page content from the current tab into a viewport within RDM
    debug("Create browser swapper");
    this.swap = swapToInnerBrowser({
      tab: this.tab,
      containerURL: TOOL_URL,
      async getInnerBrowser(containerBrowser) {
        let toolWindow = ui.toolWindow = containerBrowser.contentWindow;
        toolWindow.addEventListener("message", ui);
        debug("Wait until init from inner");
        await message.request(toolWindow, "init");
        toolWindow.addInitialViewport("about:blank");
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
    let isWindowClosing = options && options.reason === "unload";
    let isTabContentDestroying =
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
      reloadNeeded |= await this.updateDPPX();
      reloadNeeded |= await this.updateNetworkThrottling();
      reloadNeeded |= await this.updateUserAgent();
      reloadNeeded |= await this.updateTouchSimulation();
      if (reloadNeeded) {
        this.getViewportBrowser().reload();
      }
    }

    // Destroy local state
    let swap = this.swap;
    this.browserWindow = null;
    this.tab = null;
    this.inited = null;
    this.toolWindow = null;
    this.swap = null;

    // Close the debugger client used to speak with emulation actor.
    // The actor handles clearing any overrides itself, so it's not necessary to clear
    // anything on shutdown client side.
    let clientClosed = this.client.close();
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
    let { tab } = await this.client.getTab();
    this.emulationFront = EmulationFront(this.client, tab);
  },

  handleEvent(event) {
    let { browserWindow, tab } = this;

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
      case "change-network-throtting":
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
    let { userAgent, pixelRatio, touch } = event.data.device;
    // Bug 1428799: Should we reload on UA change as well?
    await this.updateUserAgent(userAgent);
    await this.updateDPPX(pixelRatio);
    let reloadNeeded = await this.updateTouchSimulation(touch);
    if (reloadNeeded) {
      this.getViewportBrowser().reload();
    }
    // Used by tests
    this.emit("device-changed");
  },

  async onChangeNetworkThrottling(event) {
    let { enabled, profile } = event.data;
    await this.updateNetworkThrottling(enabled, profile);
    // Used by tests
    this.emit("network-throttling-changed");
  },

  onChangePixelRatio(event) {
    let { pixelRatio } = event.data;
    this.updateDPPX(pixelRatio);
  },

  async onChangeTouchSimulation(event) {
    let { enabled } = event.data;
    let reloadNeeded = await this.updateTouchSimulation(enabled);
    if (reloadNeeded) {
      this.getViewportBrowser().reload();
    }
    // Used by tests
    this.emit("touch-simulation-changed");
  },

  onContentResize(event) {
    let { width, height } = event.data;
    this.emit("content-resize", {
      width,
      height,
    });
  },

  onExit() {
    let { browserWindow, tab } = this;
    ResponsiveUIManager.closeIfNeeded(browserWindow, tab);
  },

  async onRemoveDeviceAssociation(event) {
    // Bug 1428799: Should we reload on UA change as well?
    await this.updateUserAgent();
    await this.updateDPPX();
    let reloadNeeded = await this.updateTouchSimulation();
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
    let data = throttlingProfiles.find(({ id }) => id == profile);
    let { download, upload, latency } = data;
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
