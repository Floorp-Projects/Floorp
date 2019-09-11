/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const Services = require("Services");
const EventEmitter = require("devtools/shared/event-emitter");
const { getOrientation } = require("./utils/orientation");

loader.lazyRequireGetter(
  this,
  "DebuggerClient",
  "devtools/shared/client/debugger-client",
  true
);
loader.lazyRequireGetter(
  this,
  "DebuggerServer",
  "devtools/server/debugger-server",
  true
);
loader.lazyRequireGetter(
  this,
  "throttlingProfiles",
  "devtools/client/shared/components/throttling/profiles"
);
loader.lazyRequireGetter(
  this,
  "swapToInnerBrowser",
  "devtools/client/responsive/browser/swap",
  true
);
loader.lazyRequireGetter(
  this,
  "message",
  "devtools/client/responsive/utils/message"
);
loader.lazyRequireGetter(
  this,
  "showNotification",
  "devtools/client/responsive/utils/notification",
  true
);
loader.lazyRequireGetter(this, "l10n", "devtools/client/responsive/utils/l10n");
loader.lazyRequireGetter(this, "asyncStorage", "devtools/shared/async-storage");

const TOOL_URL = "chrome://devtools/content/responsive/index.xhtml";

const RELOAD_CONDITION_PREF_PREFIX = "devtools.responsive.reloadConditions.";
const RELOAD_NOTIFICATION_PREF =
  "devtools.responsive.reloadNotification.enabled";

function debug(msg) {
  // console.log(`RDM manager: ${msg}`);
}

/**
 * ResponsiveUI manages the responsive design tool for a specific tab.  The
 * actual tool itself lives in a separate chrome:// document that is loaded into
 * the tab upon opening responsive design.  This object acts a helper to
 * integrate the tool into the surrounding browser UI as needed.
 */
function ResponsiveUI(manager, window, tab) {
  this.manager = manager;
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
        const toolWindow = (ui.toolWindow = containerBrowser.contentWindow);
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
      },
    });
    debug("Wait until swap start");
    await this.swap.start();

    // Set the ui toolWindow to fullZoom and textZoom of 100%. Directly change
    // the zoom levels of the toolwindow docshell. That doesn't affect the zoom
    // of the RDM content, but it does send events that confuse the Zoom UI.
    // So before we adjust the zoom levels of the toolWindow, we first cache
    // the reported zoom levels of the RDM content, because we'll have to
    // re-apply them to re-sync the Zoom UI.

    // Cache the values now and we'll re-apply them near the end of this function.
    // This is important since other steps here can also cause the Zoom UI update
    // event to be sent for other browsers, and this means that the changes from
    // our Zoom UI update event would be overwritten. After this function, future
    // changes to zoom levels will send Zoom UI update events in an order that
    // keeps the Zoom UI synchronized with the RDM content zoom levels.
    const rdmContent = this.tab.linkedBrowser;
    const rdmViewport = ui.toolWindow;

    const fullZoom = rdmContent.fullZoom;
    const textZoom = rdmContent.textZoom;

    rdmViewport.docShell.contentViewer.fullZoom = 1;
    rdmViewport.docShell.contentViewer.textZoom = 1;

    // Listen to FullZoomChange events coming from the linkedBrowser,
    // so that we can zoom the size of the viewport by the same amount.
    rdmContent.addEventListener("FullZoomChange", this);

    this.tab.addEventListener("BeforeTabRemotenessChange", this);

    // Notify the inner browser to start the frame script
    debug("Wait until start frame script");
    await message.request(this.toolWindow, "start-frame-script");

    // Get the protocol ready to speak with emulation actor
    debug("Wait until RDP server connect");
    await this.connectToServer();

    // Restore the previous state of RDM.
    await this.restoreState();

    // Re-apply our cached zoom levels. Other Zoom UI update events have finished
    // by now.
    rdmContent.fullZoom = fullZoom;
    rdmContent.textZoom = textZoom;

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
    const isTabDestroyed = !this.tab.linkedBrowser;
    const isWindowClosing =
      (options && options.reason === "unload") || isTabDestroyed;
    const isTabContentDestroying =
      isWindowClosing ||
      (options &&
        (options.reason === "TabClose" ||
          options.reason === "BeforeTabRemotenessChange"));

    // Ensure init has finished before starting destroy
    if (!isTabContentDestroying) {
      await this.inited;
    }

    this.tab.linkedBrowser.removeEventListener("FullZoomChange", this);
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
      reloadNeeded |=
        (await this.updateUserAgent()) && this.reloadOnChange("userAgent");
      reloadNeeded |=
        (await this.updateTouchSimulation()) &&
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
    // The client being instantiated here is separate from the toolbox. It is being used
    // separately and has a life cycle that doesn't correspond to the toolbox.
    DebuggerServer.init();
    DebuggerServer.registerAllActors();
    this.client = new DebuggerClient(DebuggerServer.connectPipe());
    await this.client.connect();
    const targetFront = await this.client.mainRoot.getTab();
    this.emulationFront = await targetFront.getFront("emulation");
  },

  /**
   * Show one-time notification about reloads for emulation.
   */
  showReloadNotification() {
    if (Services.prefs.getBoolPref(RELOAD_NOTIFICATION_PREF, false)) {
      showNotification(this.browserWindow, this.tab, {
        msg: l10n.getFormatStr("responsive.reloadNotification.description2"),
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
    const { browserWindow, tab, toolWindow } = this;

    switch (event.type) {
      case "message":
        this.handleMessage(event);
        break;
      case "FullZoomChange":
        const zoom = tab.linkedBrowser.fullZoom;
        toolWindow.setViewportZoom(zoom);
        break;
      case "BeforeTabRemotenessChange":
      case "TabClose":
      case "unload":
        this.manager.closeIfNeeded(browserWindow, tab, {
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
      case "change-user-agent":
        this.onChangeUserAgent(event);
        break;
      case "content-resize":
        this.onContentResize(event);
        break;
      case "exit":
        this.onExit();
        break;
      case "remove-device-association":
        this.onRemoveDeviceAssociation();
        break;
      case "viewport-orientation-change":
        this.onRotateViewport(event);
        break;
      case "viewport-resize":
        this.onResizeViewport(event);
        break;
    }
  },

  async onChangeDevice(event) {
    const { pixelRatio, touch, userAgent } = event.data.device;
    let reloadNeeded = false;
    await this.updateDPPX(pixelRatio);

    // Get the orientation values of the device we are changing to and update.
    const { device, viewport } = event.data;
    const { type, angle } = getOrientation(device, viewport);
    await this.updateScreenOrientation(type, angle);

    reloadNeeded |=
      (await this.updateUserAgent(userAgent)) &&
      this.reloadOnChange("userAgent");
    reloadNeeded |=
      (await this.updateTouchSimulation(touch)) &&
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
    const reloadNeeded =
      (await this.updateTouchSimulation(enabled)) &&
      this.reloadOnChange("touchSimulation");
    if (reloadNeeded) {
      this.getViewportBrowser().reload();
    }
    // Used by tests
    this.emit("touch-simulation-changed");
  },

  async onChangeUserAgent(event) {
    const { userAgent } = event.data;
    const reloadNeeded =
      (await this.updateUserAgent(userAgent)) &&
      this.reloadOnChange("userAgent");
    if (reloadNeeded) {
      this.getViewportBrowser().reload();
    }
    this.emit("user-agent-changed");
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
    this.manager.closeIfNeeded(browserWindow, tab);
  },

  async onRemoveDeviceAssociation() {
    let reloadNeeded = false;
    await this.updateDPPX();
    reloadNeeded |=
      (await this.updateUserAgent()) && this.reloadOnChange("userAgent");
    reloadNeeded |=
      (await this.updateTouchSimulation()) &&
      this.reloadOnChange("touchSimulation");
    if (reloadNeeded) {
      this.getViewportBrowser().reload();
    }
    // Used by tests
    this.emit("device-association-removed");
  },

  onResizeViewport(event) {
    const { width, height } = event.data;
    this.emit("viewport-resize", {
      width,
      height,
    });
  },

  async onRotateViewport(event) {
    const { orientationType: type, angle, isViewportRotated } = event.data;
    await this.updateScreenOrientation(type, angle, isViewportRotated);
  },

  /**
   * Restores the previous state of RDM.
   */
  async restoreState() {
    const deviceState = await asyncStorage.getItem(
      "devtools.responsive.deviceState"
    );
    if (deviceState) {
      // Return if there is a device state to restore, this will be done when the
      // device list is loaded after the post-init.
      return;
    }

    const height = Services.prefs.getIntPref(
      "devtools.responsive.viewport.height",
      0
    );
    const pixelRatio = Services.prefs.getIntPref(
      "devtools.responsive.viewport.pixelRatio",
      0
    );
    const touchSimulationEnabled = Services.prefs.getBoolPref(
      "devtools.responsive.touchSimulation.enabled",
      false
    );
    const userAgent = Services.prefs.getCharPref(
      "devtools.responsive.userAgent",
      ""
    );
    const width = Services.prefs.getIntPref(
      "devtools.responsive.viewport.width",
      0
    );

    let reloadNeeded = false;
    const { type, angle } = this.getInitialViewportOrientation({
      width,
      height,
    });

    await this.updateDPPX(pixelRatio);
    await this.updateScreenOrientation(type, angle);

    if (touchSimulationEnabled) {
      reloadNeeded |=
        (await this.updateTouchSimulation(touchSimulationEnabled)) &&
        this.reloadOnChange("touchSimulation");
    }
    if (userAgent) {
      reloadNeeded |=
        (await this.updateUserAgent(userAgent)) &&
        this.reloadOnChange("userAgent");
    }
    if (reloadNeeded) {
      this.getViewportBrowser().reload();
    }
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
   * Set or clear touch simulation. When setting to true, this method will
   * additionally set meta viewport override if the pref
   * "devtools.responsive.metaViewport.enabled" is true. When setting to
   * false, this method will clear all touch simulation and meta viewport
   * overrides, returning to default behavior for both settings.
   *
   * @return boolean
   *         Whether a reload is needed to apply the override change(s).
   */
  async updateTouchSimulation(enabled) {
    let reloadNeeded;
    if (enabled) {
      const metaViewportEnabled = Services.prefs.getBoolPref(
        "devtools.responsive.metaViewport.enabled",
        false
      );

      reloadNeeded = await this.emulationFront.setTouchEventsOverride(
        Ci.nsIDocShell.TOUCHEVENTS_OVERRIDE_ENABLED
      );

      if (metaViewportEnabled) {
        reloadNeeded |= await this.emulationFront.setMetaViewportOverride(
          Ci.nsIDocShell.META_VIEWPORT_OVERRIDE_ENABLED
        );
      }
    } else {
      reloadNeeded = await this.emulationFront.clearTouchEventsOverride();
      reloadNeeded |= await this.emulationFront.clearMetaViewportOverride();
    }
    return reloadNeeded;
  },

  /**
   * Sets the screen orientation values of the simulated device.
   *
   * @param {String} type
   *        The orientation type to update the current device screen to.
   * @param {Number} angle
   *        The rotation angle to update the current device screen to.
   * @param {Boolean} isViewportRotated
   *        Whether or not the reason for updating the screen orientation is a result
   *        of actually rotating the device via the RDM toolbar. If true, then an
   *        "orientationchange" event is simulated. Otherwise, the screen orientation is
   *        updated because of changing devices, opening RDM, or the page has been
   *        reloaded/navigated to, so we should not be simulating "orientationchange".
   */
  async updateScreenOrientation(type, angle, isViewportRotated = false) {
    const targetFront = await this.client.mainRoot.getTab();
    const simulateOrientationChangeSupported = await targetFront.actorHasMethod(
      "emulation",
      "simulateScreenOrientationChange"
    );

    // Ensure that simulateScreenOrientationChange is supported.
    if (simulateOrientationChangeSupported) {
      await this.emulationFront.simulateScreenOrientationChange(
        type,
        angle,
        isViewportRotated
      );
    }

    // Used by tests.
    if (!isViewportRotated) {
      this.emit("only-viewport-orientation-changed");
    }
  },

  /**
   * Helper for tests. Assumes a single viewport for now.
   */
  getViewportSize() {
    return this.toolWindow.getViewportSize();
  },

  /**
   * Helper for tests, etc. Assumes a single viewport for now.
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

  /**
   * Helper for getting the initial viewport orientation.
   */
  getInitialViewportOrientation(viewport) {
    return getOrientation(viewport, viewport);
  },
};

EventEmitter.decorate(ResponsiveUI.prototype);

module.exports = ResponsiveUI;
