/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const Services = require("Services");
const EventEmitter = require("devtools/shared/event-emitter");
const {
  getOrientation,
} = require("devtools/client/responsive/utils/orientation");
const Constants = require("devtools/client/responsive/constants");
const {
  ResourceWatcher,
} = require("devtools/shared/resources/resource-watcher");
const {
  CommandsFactory,
} = require("devtools/shared/commands/commands-factory");

loader.lazyRequireGetter(
  this,
  "throttlingProfiles",
  "devtools/client/shared/components/throttling/profiles"
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
loader.lazyRequireGetter(
  this,
  "PriorityLevels",
  "devtools/client/shared/components/NotificationBox",
  true
);
loader.lazyRequireGetter(this, "l10n", "devtools/client/responsive/utils/l10n");
loader.lazyRequireGetter(this, "asyncStorage", "devtools/shared/async-storage");
loader.lazyRequireGetter(
  this,
  "captureAndSaveScreenshot",
  "devtools/client/shared/screenshot",
  true
);

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
class ResponsiveUI {
  /**
   * @param {ResponsiveUIManager} manager
   *        The ResponsiveUIManager instance.
   * @param {ChromeWindow} window
   *        The main browser chrome window (that holds many tabs).
   * @param {Tab} tab
   *        The specific browser <tab> element this responsive instance is for.
   */
  constructor(manager, window, tab) {
    this.manager = manager;
    // The main browser chrome window (that holds many tabs).
    this.browserWindow = window;
    // The specific browser tab this responsive instance is for.
    this.tab = tab;

    // Flag set when destruction has begun.
    this.destroying = false;
    // Flag set when destruction has ended.
    this.destroyed = false;
    // The iframe containing the RDM UI.
    this.rdmFrame = null;

    // Bind callbacks for resizers.
    this.onResizeDrag = this.onResizeDrag.bind(this);
    this.onResizeStart = this.onResizeStart.bind(this);
    this.onResizeStop = this.onResizeStop.bind(this);

    this.onTargetAvailable = this.onTargetAvailable.bind(this);

    this.networkFront = null;
    // Promise resovled when the UI init has completed.
    this.inited = this.init();

    EventEmitter.decorate(this);
  }

  get toolWindow() {
    return this.rdmFrame.contentWindow;
  }

  get docShell() {
    return this.toolWindow.docShell;
  }

  get viewportElement() {
    return this.browserStackEl.querySelector("browser");
  }

  get currentTarget() {
    return this.commands.targetCommand.targetFront;
  }

  get watcherFront() {
    return this.resourceWatcher.watcherFront;
  }

  /**
   * Open RDM while preserving the state of the page.
   */
  async init() {
    debug("Init start");

    this.initRDMFrame();

    // Hide the browser content temporarily while things move around to avoid displaying
    // strange intermediate states.
    this.hideBrowserUI();

    // Watch for tab close and window close so we can clean up RDM synchronously
    this.tab.addEventListener("TabClose", this);
    this.browserWindow.addEventListener("unload", this);
    this.rdmFrame.contentWindow.addEventListener("message", this);

    this.tab.linkedBrowser.enterResponsiveMode();

    // Listen to FullZoomChange events coming from the browser window,
    // so that we can zoom the size of the viewport by the same amount.
    this.browserWindow.addEventListener("FullZoomChange", this);

    // Get the protocol ready to speak with responsive emulation actor
    debug("Wait until RDP server connect");
    await this.connectToServer();

    // Restore the previous UI state.
    await this.restoreUIState();

    // Show the browser UI now that its state is ready.
    this.showBrowserUI();

    // Non-blocking message to tool UI to start any delayed init activities
    message.post(this.toolWindow, "post-init");

    debug("Init done");
  }

  /**
   * Initialize the RDM iframe inside of the browser document.
   */
  initRDMFrame() {
    const { document: doc, gBrowser } = this.browserWindow;
    const rdmFrame = doc.createElement("iframe");
    rdmFrame.src = "chrome://devtools/content/responsive/toolbar.xhtml";
    rdmFrame.classList.add("rdm-toolbar");

    // Create resizer handlers
    const resizeHandle = doc.createElement("div");
    resizeHandle.classList.add("viewport-resize-handle");
    const resizeHandleX = doc.createElement("div");
    resizeHandleX.classList.add("viewport-horizontal-resize-handle");
    const resizeHandleY = doc.createElement("div");
    resizeHandleY.classList.add("viewport-vertical-resize-handle");

    this.browserContainerEl = gBrowser.getBrowserContainer(
      gBrowser.getBrowserForTab(this.tab)
    );
    this.browserStackEl = this.browserContainerEl.querySelector(
      ".browserStack"
    );

    this.browserContainerEl.classList.add("responsive-mode");

    // Prepend the RDM iframe inside of the current tab's browser stack.
    this.browserStackEl.prepend(rdmFrame);
    this.browserStackEl.append(resizeHandle);
    this.browserStackEl.append(resizeHandleX);
    this.browserStackEl.append(resizeHandleY);

    // Wait for the frame script to be loaded.
    message.wait(rdmFrame.contentWindow, "script-init").then(async () => {
      // Notify the frame window that the Resposnive UI manager has begun initializing.
      // At this point, we can render our React content inside the frame.
      message.post(rdmFrame.contentWindow, "init");
      // Wait for the tools to be rendered above the content. The frame script will
      // then dispatch the necessary actions to the Redux store to give the toolbar the
      // state it needs.
      message.wait(rdmFrame.contentWindow, "init:done").then(() => {
        rdmFrame.contentWindow.addInitialViewport({
          userContextId: this.tab.userContextId,
        });
      });
    });

    this.rdmFrame = rdmFrame;

    this.resizeHandle = resizeHandle;
    this.resizeHandle.addEventListener("mousedown", this.onResizeStart);

    this.resizeHandleX = resizeHandleX;
    this.resizeHandleX.addEventListener("mousedown", this.onResizeStart);

    this.resizeHandleY = resizeHandleY;
    this.resizeHandleY.addEventListener("mousedown", this.onResizeStart);

    // Setup a ResizeObserver that stores the width and height of the
    // .browserStack size as properties. These set properties are then used
    // to out-of-grid elements that are affected by RDM.
    this.resizeToolbarObserver = new this.browserWindow.ResizeObserver(() => {
      const style = this.browserWindow.getComputedStyle(this.browserStackEl);

      this.browserStackEl.style.setProperty("--rdm-stack-width", style.width);
      this.browserStackEl.style.setProperty("--rdm-stack-height", style.height);
      // If the toolbar needs extra space for the UA input, then set a class that
      // will accomodate its height. We should also make sure to keep the width
      // value we're toggling against in sync with the media-query in
      // devtools/client/responsive/index.css
      this.rdmFrame.classList.toggle(
        "accomodate-ua",
        parseFloat(style.width) < 520
      );
    });

    this.resizeToolbarObserver.observe(this.browserStackEl);
  }

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
    const isTabDestroyed =
      !this.tab.linkedBrowser || this.responsiveFront.isDestroyed();
    const isWindowClosing = options?.reason === "unload" || isTabDestroyed;
    const isTabContentDestroying =
      isWindowClosing || options?.reason === "TabClose";

    let currentTarget;

    // Ensure init has finished before starting destroy
    if (!isTabContentDestroying) {
      await this.inited;

      // Restore screen orientation of physical device.
      await this.updateScreenOrientation("landscape-primary", 0);
      await this.updateMaxTouchPointsEnabled(false);
      await this.responsiveFront.setFloatingScrollbars(false);

      // Hide browser UI to avoid displaying weird intermediate states while closing.
      this.hideBrowserUI();

      // Save reference to tab target before RDM stops listening to it. Will need it if
      // the tab has to be reloaded to remove the emulated settings created by RDM.
      currentTarget = this.currentTarget;

      // Resseting the throtting needs to be done before the
      // network events watching is stopped.
      await this.updateNetworkThrottling();
    }

    this.tab.removeEventListener("TabClose", this);
    this.browserWindow.removeEventListener("unload", this);
    this.tab.linkedBrowser.leaveResponsiveMode();

    this.browserWindow.removeEventListener("FullZoomChange", this);
    this.rdmFrame.contentWindow.removeEventListener("message", this);

    // Remove observers on the stack.
    this.resizeToolbarObserver.unobserve(this.browserStackEl);

    this.rdmFrame.remove();

    // Clean up resize handlers
    this.resizeHandle.remove();
    this.resizeHandleX.remove();
    this.resizeHandleY.remove();

    this.browserContainerEl.classList.remove("responsive-mode");
    this.browserStackEl.style.removeProperty("--rdm-width");
    this.browserStackEl.style.removeProperty("--rdm-height");
    this.browserStackEl.style.removeProperty("--rdm-zoom");
    this.browserStackEl.style.removeProperty("--rdm-stack-height");
    this.browserStackEl.style.removeProperty("--rdm-stack-width");

    // Ensure the tab is reloaded if required when exiting RDM so that no emulated
    // settings are left in a customized state.
    if (!isTabContentDestroying) {
      let reloadNeeded = false;
      await this.updateDPPX(null);
      reloadNeeded |=
        (await this.updateUserAgent()) && this.reloadOnChange("userAgent");
      reloadNeeded |=
        (await this.updateTouchSimulation()) &&
        this.reloadOnChange("touchSimulation");
      if (reloadNeeded && currentTarget) {
        await currentTarget.reload();
      }

      // Unwatch targets & resources as the last step. If we are not waching for
      // any resource & target anymore, the JSWindowActors will be unregistered
      // which will trigger an early destruction of the RDM target, before we
      // could finalize the cleanup.
      this.commands.targetCommand.unwatchTargets(
        [this.commands.targetCommand.TYPES.FRAME],
        this.onTargetAvailable
      );

      this.resourceWatcher.unwatchResources(
        [this.resourceWatcher.TYPES.NETWORK_EVENT],
        { onAvailable: this.onNetworkResourceAvailable }
      );

      this.commands.targetCommand.destroy();
    }

    // Show the browser UI now.
    this.showBrowserUI();

    // Destroy local state
    this.browserContainerEl = null;
    this.browserStackEl = null;
    this.browserWindow = null;
    this.tab = null;
    this.inited = null;
    this.rdmFrame = null;
    this.resizeHandle = null;
    this.resizeHandleX = null;
    this.resizeHandleY = null;
    this.resizeToolbarObserver = null;

    // Destroying the commands will close the devtools client used to speak with responsive emulation actor.
    // The actor handles clearing any overrides itself, so it's not necessary to clear
    // anything on shutdown client side.
    const commandsDestroyed = this.commands.destroy();
    if (!isTabContentDestroying) {
      await commandsDestroyed;
    }
    this.commands = this.responsiveFront = null;
    this.destroyed = true;

    return true;
  }

  async connectToServer() {
    this.commands = await CommandsFactory.forTab(this.tab);
    this.resourceWatcher = new ResourceWatcher(this.commands.targetCommand);

    await this.commands.targetCommand.startListening();

    await this.commands.targetCommand.watchTargets(
      [this.commands.targetCommand.TYPES.FRAME],
      this.onTargetAvailable
    );

    // To support network throttling the resource watcher
    // needs to be watching for network resources.
    await this.resourceWatcher.watchResources(
      [this.resourceWatcher.TYPES.NETWORK_EVENT],
      { onAvailable: this.onNetworkResourceAvailable }
    );

    this.networkFront = await this.watcherFront.getNetworkParentActor();
  }

  /**
   * Show one-time notification about reloads for responsive emulation.
   */
  showReloadNotification() {
    if (Services.prefs.getBoolPref(RELOAD_NOTIFICATION_PREF, false)) {
      showNotification(this.browserWindow, this.tab, {
        msg: l10n.getFormatStr("responsive.reloadNotification.description2"),
      });
      Services.prefs.setBoolPref(RELOAD_NOTIFICATION_PREF, false);
    }
  }

  reloadOnChange(id) {
    this.showReloadNotification();
    const pref = RELOAD_CONDITION_PREF_PREFIX + id;
    return Services.prefs.getBoolPref(pref, false);
  }

  hideBrowserUI() {
    this.tab.linkedBrowser.style.visibility = "hidden";
    this.resizeHandle.style.visibility = "hidden";
  }

  showBrowserUI() {
    this.tab.linkedBrowser.style.removeProperty("visibility");
    this.resizeHandle.style.removeProperty("visibility");
  }

  handleEvent(event) {
    const { browserWindow, tab } = this;

    switch (event.type) {
      case "message":
        this.handleMessage(event);
        break;
      case "FullZoomChange":
        // Get the current device size and update to that size, which
        // will pick up changes to the zoom.
        const { width, height } = this.getViewportSize();
        this.updateViewportSize(width, height);
        break;
      case "TabClose":
      case "unload":
        this.manager.closeIfNeeded(browserWindow, tab, {
          reason: event.type,
        });
        break;
    }
  }

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
      case "screenshot":
        this.onScreenshot();
        break;
      case "toggle-left-alignment":
        this.onToggleLeftAlignment(event);
        break;
      case "update-device-modal":
        this.onUpdateDeviceModal(event);
        break;
    }
  }

  async onChangeDevice(event) {
    const { pixelRatio, touch, userAgent } = event.data.device;
    let reloadNeeded = false;
    await this.updateDPPX(pixelRatio);

    // Get the orientation values of the device we are changing to and update.
    const { device, viewport } = event.data;
    const { type, angle } = getOrientation(device, viewport);
    await this.updateScreenOrientation(type, angle);
    await this.updateMaxTouchPointsEnabled(touch);

    reloadNeeded |=
      (await this.updateUserAgent(userAgent)) &&
      this.reloadOnChange("userAgent");
    reloadNeeded |=
      (await this.updateTouchSimulation(touch)) &&
      this.reloadOnChange("touchSimulation");
    if (reloadNeeded) {
      this.reloadBrowser();
    }
    // Used by tests
    this.emit("device-changed");
  }

  async onChangeNetworkThrottling(event) {
    const { enabled, profile } = event.data;
    await this.updateNetworkThrottling(enabled, profile);
    // Used by tests
    this.emit("network-throttling-changed");
  }

  onChangePixelRatio(event) {
    const { pixelRatio } = event.data;
    this.updateDPPX(pixelRatio);
  }

  async onChangeTouchSimulation(event) {
    const { enabled } = event.data;

    await this.updateMaxTouchPointsEnabled(enabled);

    const reloadNeeded =
      (await this.updateTouchSimulation(enabled)) &&
      this.reloadOnChange("touchSimulation");
    if (reloadNeeded) {
      this.reloadBrowser();
    }
    // Used by tests
    this.emit("touch-simulation-changed");
  }

  async onChangeUserAgent(event) {
    const { userAgent } = event.data;
    const reloadNeeded =
      (await this.updateUserAgent(userAgent)) &&
      this.reloadOnChange("userAgent");
    if (reloadNeeded) {
      this.reloadBrowser();
    }
    this.emit("user-agent-changed");
  }

  onExit() {
    const { browserWindow, tab } = this;
    this.manager.closeIfNeeded(browserWindow, tab);
  }

  async onRemoveDeviceAssociation() {
    let reloadNeeded = false;
    await this.updateDPPX(null);
    reloadNeeded |=
      (await this.updateUserAgent()) && this.reloadOnChange("userAgent");
    reloadNeeded |=
      (await this.updateTouchSimulation()) &&
      this.reloadOnChange("touchSimulation");
    if (reloadNeeded) {
      this.reloadBrowser();
    }
    // Used by tests
    this.emit("device-association-removed");
  }

  /**
   * Resizing the browser on mousemove
   */
  onResizeDrag({ screenX, screenY }) {
    if (!this.isResizing || !this.rdmFrame.contentWindow) {
      return;
    }

    const zoom = this.tab.linkedBrowser.fullZoom;

    let deltaX = (screenX - this.lastScreenX) / zoom;
    let deltaY = (screenY - this.lastScreenY) / zoom;

    const leftAlignmentEnabled = Services.prefs.getBoolPref(
      "devtools.responsive.leftAlignViewport.enabled",
      false
    );

    if (!leftAlignmentEnabled) {
      // The viewport is centered horizontally, so horizontal resize resizes
      // by twice the distance the mouse was dragged - on left and right side.
      deltaX = deltaX * 2;
    }

    if (this.ignoreX) {
      deltaX = 0;
    }
    if (this.ignoreY) {
      deltaY = 0;
    }

    const viewportSize = this.getViewportSize();

    let width = Math.round(viewportSize.width + deltaX);
    let height = Math.round(viewportSize.height + deltaY);

    if (width < Constants.MIN_VIEWPORT_DIMENSION) {
      width = Constants.MIN_VIEWPORT_DIMENSION;
    } else if (width != viewportSize.width) {
      this.lastScreenX = screenX;
    }

    if (height < Constants.MIN_VIEWPORT_DIMENSION) {
      height = Constants.MIN_VIEWPORT_DIMENSION;
    } else if (height != viewportSize.height) {
      this.lastScreenY = screenY;
    }

    // Update the RDM store and viewport size with the new width and height.
    this.rdmFrame.contentWindow.setViewportSize({ width, height });
    this.updateViewportSize(width, height);

    // Change the device selector back to an unselected device
    if (this.rdmFrame.contentWindow.getAssociatedDevice()) {
      this.rdmFrame.contentWindow.clearDeviceAssociation();
    }
  }

  /**
   * Start the process of resizing the browser.
   */
  onResizeStart({ target, screenX, screenY }) {
    this.browserWindow.addEventListener("mousemove", this.onResizeDrag, true);
    this.browserWindow.addEventListener("mouseup", this.onResizeStop, true);

    this.isResizing = true;
    this.lastScreenX = screenX;
    this.lastScreenY = screenY;
    this.ignoreX = target === this.resizeHandleY;
    this.ignoreY = target === this.resizeHandleX;
  }

  /**
   * Stop the process of resizing the browser.
   */
  onResizeStop() {
    this.browserWindow.removeEventListener(
      "mousemove",
      this.onResizeDrag,
      true
    );
    this.browserWindow.removeEventListener("mouseup", this.onResizeStop, true);

    this.isResizing = false;
    this.lastScreenX = 0;
    this.lastScreenY = 0;
    this.ignoreX = false;
    this.ignoreY = false;

    // Used by tests.
    this.emit("viewport-resize-dragend");
  }

  onResizeViewport(event) {
    const { width, height } = event.data;
    this.updateViewportSize(width, height);
    this.emit("viewport-resize", {
      width,
      height,
    });
  }

  async onRotateViewport(event) {
    const { orientationType: type, angle, isViewportRotated } = event.data;
    await this.updateScreenOrientation(type, angle, isViewportRotated);
  }

  async onScreenshot() {
    const messages = await captureAndSaveScreenshot(
      this.currentTarget,
      this.browserWindow
    );

    const priorityMap = {
      error: PriorityLevels.PRIORITY_CRITICAL_HIGH,
      warn: PriorityLevels.PRIORITY_WARNING_HIGH,
    };
    for (const { text, level } of messages) {
      // captureAndSaveScreenshot returns "saved" messages, that indicate where the
      // screenshot was saved. We don't want to display them as the download UI can be
      // used to open the file.
      if (level !== "warn" && level !== "error") {
        continue;
      }

      showNotification(this.browserWindow, this.tab, {
        msg: text,
        priority: priorityMap[level],
      });
    }

    message.post(this.rdmFrame.contentWindow, "screenshot-captured");
  }

  onToggleLeftAlignment(event) {
    this.updateUIAlignment(event.data.leftAlignmentEnabled);
  }

  onUpdateDeviceModal(event) {
    if (event.data.isOpen) {
      this.browserStackEl.classList.add("device-modal-opened");
    } else {
      this.browserStackEl.classList.remove("device-modal-opened");
    }
  }

  async hasDeviceState() {
    const deviceState = await asyncStorage.getItem(
      "devtools.responsive.deviceState"
    );
    return !!deviceState;
  }

  /**
   * Restores the previous UI state.
   */
  async restoreUIState() {
    const leftAlignmentEnabled = Services.prefs.getBoolPref(
      "devtools.responsive.leftAlignViewport.enabled",
      false
    );

    this.updateUIAlignment(leftAlignmentEnabled);

    const height = Services.prefs.getIntPref(
      "devtools.responsive.viewport.height",
      0
    );
    const width = Services.prefs.getIntPref(
      "devtools.responsive.viewport.width",
      0
    );
    this.updateViewportSize(width, height);
  }

  /**
   * Restores the previous actor state.
   */
  async restoreActorState() {
    // It's possible the target will switch to a page loaded in the
    // parent-process (i.e: about:robots). When this happens, the values set
    // on the BrowsingContext by RDM are not preserved. So we need to call
    // enterResponsiveMode whenever there is a target switch.
    this.tab.linkedBrowser.enterResponsiveMode();

    // Apply floating scrollbar styles to document.
    await this.responsiveFront.setFloatingScrollbars(true);

    // Attach current target to the selected browser tab.
    await this.currentTarget.attach();

    const hasDeviceState = await this.hasDeviceState();
    if (hasDeviceState) {
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

    const { type, angle } = this.getInitialViewportOrientation({
      width,
      height,
    });

    await this.updateDPPX(pixelRatio);
    await this.updateScreenOrientation(type, angle);
    await this.updateMaxTouchPointsEnabled(touchSimulationEnabled);

    let reloadNeeded = false;
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
      this.reloadBrowser();
    }
  }

  /**
   * Set or clear the emulated device pixel ratio.
   *
   * @param {Number|null} dppx: The ratio to simulate. Set to null to disable the
   *                      simulation and roll back to the original ratio
   */
  async updateDPPX(dppx = null) {
    await this.commands.targetConfigurationCommand.updateConfiguration({
      overrideDPPX: dppx,
    });
  }

  /**
   * Set or clear network throttling.
   *
   * @return boolean
   *         Whether a reload is needed to apply the change.
   *         (This is always immediate, so it's always false.)
   */
  async updateNetworkThrottling(enabled, profile) {
    if (!enabled) {
      await this.networkFront.clearNetworkThrottling();
      return false;
    }
    const data = throttlingProfiles.find(({ id }) => id == profile);
    const { download, upload, latency } = data;
    await this.networkFront.setNetworkThrottling({
      downloadThroughput: download,
      uploadThroughput: upload,
      latency,
    });
    return false;
  }

  /**
   * Set or clear the emulated user agent.
   *
   * @return boolean
   *         Whether a reload is needed to apply the change.
   */
  updateUserAgent(userAgent) {
    if (!userAgent) {
      return this.responsiveFront.clearUserAgentOverride();
    }
    return this.responsiveFront.setUserAgentOverride(userAgent);
  }

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

      reloadNeeded = await this.responsiveFront.setTouchEventsOverride(
        "enabled"
      );

      if (metaViewportEnabled) {
        reloadNeeded |= await this.responsiveFront.setMetaViewportOverride(
          Ci.nsIDocShell.META_VIEWPORT_OVERRIDE_ENABLED
        );
      }
    } else {
      reloadNeeded = await this.responsiveFront.clearTouchEventsOverride();
      reloadNeeded |= await this.responsiveFront.clearMetaViewportOverride();
    }
    return reloadNeeded;
  }

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
    await this.responsiveFront.simulateScreenOrientationChange(
      type,
      angle,
      isViewportRotated
    );

    // Used by tests.
    if (!isViewportRotated) {
      this.emit("only-viewport-orientation-changed");
    }
  }

  /**
   * Sets whether or not maximum touch points are supported for the simulated device.
   *
   * @param {Boolean} touchSimulationEnabled
   *        Whether or not touch is enabled for the simulated device.
   */
  async updateMaxTouchPointsEnabled(touchSimulationEnabled) {
    return this.responsiveFront.setMaxTouchPoints(touchSimulationEnabled);
  }

  /**
   * Sets whether or not the RDM UI should be left-aligned.
   *
   * @param {Boolean} leftAlignmentEnabled
   *        Whether or not the UI is left-aligned.
   */
  updateUIAlignment(leftAlignmentEnabled) {
    this.browserContainerEl.classList.toggle(
      "left-aligned",
      leftAlignmentEnabled
    );
  }

  /**
   * Sets the browser element to be the given width and height.
   *
   * @param {Number} width
   *        The viewport's width.
   * @param {Number} height
   *        The viewport's height.
   */
  updateViewportSize(width, height) {
    const zoom = this.tab.linkedBrowser.fullZoom;

    // Setting this with a variable on the stack instead of directly as width/height
    // on the <browser> because we'll need to use this for the alert dialog as well.
    this.browserStackEl.style.setProperty("--rdm-width", `${width}px`);
    this.browserStackEl.style.setProperty("--rdm-height", `${height}px`);
    this.browserStackEl.style.setProperty("--rdm-zoom", zoom);

    // This is a bit premature, but we emit a content-resize event here. It
    // would be preferrable to wait until the viewport is actually resized,
    // but the "resize" event is not triggered by this style change. The
    // content-resize message is only used by tests, and if needed those tests
    // can use the testing function setViewportSizeAndAwaitReflow to ensure
    // the viewport has had time to reach this size.
    this.emit("content-resize", {
      width,
      height,
    });
  }

  /**
   * Helper for tests. Assumes a single viewport for now.
   */
  getViewportSize() {
    // The getViewportSize function is loaded in index.js, and might not be
    // available yet.
    if (this.toolWindow.getViewportSize) {
      return this.toolWindow.getViewportSize();
    }

    return { width: 0, height: 0 };
  }

  /**
   * Helper for tests, etc. Assumes a single viewport for now.
   */
  async setViewportSize(size) {
    await this.inited;

    // Ensure that width and height are valid.
    let { width, height } = size;
    if (!size.width) {
      width = this.getViewportSize().width;
    }

    if (!size.height) {
      height = this.getViewportSize().height;
    }

    this.rdmFrame.contentWindow.setViewportSize({ width, height });
    this.updateViewportSize(width, height);
  }

  /**
   * Helper for tests/reloading the viewport. Assumes a single viewport for now.
   */
  getViewportBrowser() {
    return this.tab.linkedBrowser;
  }

  /**
   * Helper for contacting the viewport content. Assumes a single viewport for now.
   */
  getViewportMessageManager() {
    return this.getViewportBrowser().messageManager;
  }

  /**
   * Helper for getting the initial viewport orientation.
   */
  getInitialViewportOrientation(viewport) {
    return getOrientation(viewport, viewport);
  }

  /**
   * Helper for tests to get the browser's window.
   */
  getBrowserWindow() {
    return this.browserWindow;
  }

  async onTargetAvailable({ targetFront }) {
    if (this.destroying) {
      return;
    }

    if (targetFront.isTopLevel) {
      this.responsiveFront = await targetFront.getFront("responsive");
      await this.restoreActorState();
    }
  }
  // This just needed to setup watching for network resources,
  // to support network throttling.
  onNetworkResourceAvailable() {}

  /**
   * Reload the current tab.
   */
  async reloadBrowser() {
    await this.currentTarget.reload();
  }
}

module.exports = ResponsiveUI;
