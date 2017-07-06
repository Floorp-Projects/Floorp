/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;

const { loader, require } = Cu.import("resource://devtools/shared/Loader.jsm", {});
const { LocalizationHelper } = require("devtools/shared/l10n");
const { Task } = require("devtools/shared/task");
const Services = require("Services");
const EventEmitter = require("devtools/shared/event-emitter");

loader.lazyImporter(this, "SystemAppProxy",
                    "resource://gre/modules/SystemAppProxy.jsm");
loader.lazyImporter(this, "BrowserUtils",
                    "resource://gre/modules/BrowserUtils.jsm");
loader.lazyRequireGetter(this, "Telemetry", "devtools/client/shared/telemetry");
loader.lazyRequireGetter(this, "showDoorhanger",
                         "devtools/client/shared/doorhanger", true);
loader.lazyRequireGetter(this, "gDevToolsBrowser",
                         "devtools/client/framework/devtools-browser", true);
loader.lazyRequireGetter(this, "TouchEventSimulator",
                         "devtools/shared/touch/simulator", true);
loader.lazyRequireGetter(this, "flags",
                         "devtools/shared/flags");
loader.lazyRequireGetter(this, "EmulationFront",
                         "devtools/shared/fronts/emulation", true);
loader.lazyRequireGetter(this, "DebuggerClient",
                         "devtools/shared/client/main", true);
loader.lazyRequireGetter(this, "DebuggerServer",
                         "devtools/server/main", true);
loader.lazyRequireGetter(this, "system", "devtools/shared/system");

this.EXPORTED_SYMBOLS = ["ResponsiveUIManager"];

const NEW_RDM_ENABLED = "devtools.responsive.html.enabled";

const MIN_WIDTH = 50;
const MIN_HEIGHT = 50;

const MAX_WIDTH = 10000;
const MAX_HEIGHT = 10000;

const SLOW_RATIO = 6;
const ROUND_RATIO = 10;

const INPUT_PARSER = /(\d+)[^\d]+(\d+)/;

const SHARED_L10N = new LocalizationHelper("devtools/client/locales/shared.properties");

function debug(msg) {
  // dump(`RDM UI: ${msg}\n`);
}

var ActiveTabs = new Map();

var Manager = {
  /**
   * Check if the a tab is in a responsive mode.
   * Leave the responsive mode if active,
   * active the responsive mode if not active.
   *
   * @param window the main window.
   * @param tab the tab targeted.
   */
  toggle: function (window, tab) {
    if (this.isActiveForTab(tab)) {
      ActiveTabs.get(tab).close();
    } else {
      this.openIfNeeded(window, tab);
    }
  },

  /**
   * Launches the responsive mode.
   *
   * @param window the main window.
   * @param tab the tab targeted.
   * @returns {ResponsiveUI} the instance of ResponsiveUI for the current tab.
   */
  openIfNeeded: Task.async(function* (window, tab) {
    let ui;
    if (!this.isActiveForTab(tab)) {
      ui = new ResponsiveUI(window, tab);
      yield ui.inited;
    } else {
      ui = this.getResponsiveUIForTab(tab);
    }
    return ui;
  }),

  /**
   * Returns true if responsive view is active for the provided tab.
   *
   * @param tab the tab targeted.
   */
  isActiveForTab: function (tab) {
    return ActiveTabs.has(tab);
  },

  /**
   * Return the responsive UI controller for a tab.
   */
  getResponsiveUIForTab: function (tab) {
    return ActiveTabs.get(tab);
  },

  /**
   * Handle gcli commands.
   *
   * @param window the browser window.
   * @param tab the tab targeted.
   * @param command the command name.
   * @param args command arguments.
   */
  handleGcliCommand: Task.async(function* (window, tab, command, args) {
    switch (command) {
      case "resize to":
        let ui = yield this.openIfNeeded(window, tab);
        ui.setViewportSize(args);
        break;
      case "resize on":
        this.openIfNeeded(window, tab);
        break;
      case "resize off":
        if (this.isActiveForTab(tab)) {
          yield ActiveTabs.get(tab).close();
        }
        break;
      case "resize toggle":
        this.toggle(window, tab);
        break;
      default:
    }
  })
};

EventEmitter.decorate(Manager);

// If the new HTML RDM UI is enabled and e10s is enabled by default (e10s is required for
// the new HTML RDM UI to function), delegate the ResponsiveUIManager API over to that
// tool instead.  Performing this delegation here allows us to contain the pref check to a
// single place.
if (Services.prefs.getBoolPref(NEW_RDM_ENABLED) &&
    Services.appinfo.browserTabsRemoteAutostart) {
  let { ResponsiveUIManager } =
    require("devtools/client/responsive.html/manager");
  this.ResponsiveUIManager = ResponsiveUIManager;
} else {
  this.ResponsiveUIManager = Manager;
}

var defaultPresets = [
  // Phones
  {key: "320x480", width: 320, height: 480},   // iPhone, B2G, with <meta viewport>
  {key: "360x640", width: 360, height: 640},   // Android 4, phones, with <meta viewport>

  // Tablets
  {key: "768x1024", width: 768, height: 1024}, // iPad, with <meta viewport>
  {key: "800x1280", width: 800, height: 1280}, // Android 4, Tablet, with <meta viewport>

  // Default width for mobile browsers, no <meta viewport>
  {key: "980x1280", width: 980, height: 1280},

  // Computer
  {key: "1280x600", width: 1280, height: 600},
  {key: "1920x900", width: 1920, height: 900},
];

function ResponsiveUI(window, tab) {
  this.mainWindow = window;
  this.tab = tab;
  this.mm = this.tab.linkedBrowser.messageManager;
  this.tabContainer = window.gBrowser.tabContainer;
  this.browser = tab.linkedBrowser;
  this.chromeDoc = window.document;
  this.container = window.gBrowser.getBrowserContainer(this.browser);
  this.stack = this.container.querySelector(".browserStack");
  this._telemetry = new Telemetry();

  // Let's bind some callbacks.
  this.boundPresetSelected = this.presetSelected.bind(this);
  this.boundHandleManualInput = this.handleManualInput.bind(this);
  this.boundAddPreset = this.addPreset.bind(this);
  this.boundRemovePreset = this.removePreset.bind(this);
  this.boundRotate = this.rotate.bind(this);
  this.boundScreenshot = () => this.screenshot();
  this.boundTouch = this.toggleTouch.bind(this);
  this.boundClose = this.close.bind(this);
  this.boundStartResizing = this.startResizing.bind(this);
  this.boundStopResizing = this.stopResizing.bind(this);
  this.boundOnDrag = this.onDrag.bind(this);
  this.boundChangeUA = this.changeUA.bind(this);
  this.boundOnContentResize = this.onContentResize.bind(this);

  this.mm.addMessageListener("ResponsiveMode:OnContentResize",
                             this.boundOnContentResize);

  // We must be ready to handle window or tab close now that we have saved
  // ourselves in ActiveTabs.  Otherwise we risk leaking the window.
  this.mainWindow.addEventListener("unload", this);
  this.tab.addEventListener("TabClose", this);
  this.tabContainer.addEventListener("TabSelect", this);

  ActiveTabs.set(this.tab, this);

  this.inited = this.init();
}

ResponsiveUI.prototype = {
  _transitionsEnabled: true,
  get transitionsEnabled() {
    return this._transitionsEnabled;
  },
  set transitionsEnabled(value) {
    this._transitionsEnabled = value;
    if (value && !this._resizing && this.stack.hasAttribute("responsivemode")) {
      this.stack.removeAttribute("notransition");
    } else if (!value) {
      this.stack.setAttribute("notransition", "true");
    }
  },

  init: Task.async(function* () {
    debug("INIT BEGINS");
    let ready = this.waitForMessage("ResponsiveMode:ChildScriptReady");
    this.mm.loadFrameScript("resource://devtools/client/responsivedesign/responsivedesign-child.js", true);
    yield ready;

    yield gDevToolsBrowser.loadBrowserStyleSheet(this.mainWindow);

    let requiresFloatingScrollbars =
      !this.mainWindow.matchMedia("(-moz-overlay-scrollbars)").matches;
    let started = this.waitForMessage("ResponsiveMode:Start:Done");
    debug("SEND START");
    this.mm.sendAsyncMessage("ResponsiveMode:Start", {
      requiresFloatingScrollbars,
      // Tests expect events on resize to yield on various size changes
      notifyOnResize: flags.testing,
    });
    yield started;

    // Load Presets
    this.loadPresets();

    // Setup the UI
    this.container.setAttribute("responsivemode", "true");
    this.stack.setAttribute("responsivemode", "true");
    this.buildUI();
    this.checkMenus();

    // Rotate the responsive mode if needed
    try {
      if (Services.prefs.getBoolPref("devtools.responsiveUI.rotate")) {
        this.rotate();
      }
    } catch (e) {
      // There is no default value defined, so errors are expected.
    }

    // Touch events support
    this.touchEnableBefore = false;
    this.touchEventSimulator = new TouchEventSimulator(this.browser);

    yield this.connectToServer();
    this.userAgentInput.hidden = false;

    // Hook to display promotional Developer Edition doorhanger.
    // Only displayed once.
    showDoorhanger({
      window: this.mainWindow,
      type: "deveditionpromo",
      anchor: this.chromeDoc.querySelector("#content")
    });

    this.showNewUINotification();

    // Notify that responsive mode is on.
    this._telemetry.toolOpened("responsive");
    ResponsiveUIManager.emit("on", { tab: this.tab });
  }),

  connectToServer: Task.async(function* () {
    if (!DebuggerServer.initialized) {
      DebuggerServer.init();
      DebuggerServer.addBrowserActors();
    }
    this.client = new DebuggerClient(DebuggerServer.connectPipe());
    yield this.client.connect();
    let { tab } = yield this.client.getTab();
    yield this.client.attachTab(tab.actor);
    this.emulationFront = EmulationFront(this.client, tab);
  }),

  loadPresets: function () {
    // Try to load presets from prefs
    let presets = defaultPresets;
    if (Services.prefs.prefHasUserValue("devtools.responsiveUI.presets")) {
      try {
        presets = JSON.parse(Services.prefs.getCharPref("devtools.responsiveUI.presets"));
      } catch (e) {
        // User pref is malformated.
        console.error("Could not parse pref `devtools.responsiveUI.presets`: " + e);
      }
    }

    this.customPreset = { key: "custom", custom: true };

    if (Array.isArray(presets)) {
      this.presets = [this.customPreset].concat(presets);
    } else {
      console.error("Presets value (devtools.responsiveUI.presets) is malformated.");
      this.presets = [this.customPreset];
    }

    try {
      let width = Services.prefs.getIntPref("devtools.responsiveUI.customWidth");
      let height = Services.prefs.getIntPref("devtools.responsiveUI.customHeight");
      this.customPreset.width = Math.min(MAX_WIDTH, width);
      this.customPreset.height = Math.min(MAX_HEIGHT, height);

      this.currentPresetKey =
        Services.prefs.getCharPref("devtools.responsiveUI.currentPreset");
    } catch (e) {
      // Default size. The first preset (custom) is the one that will be used.
      let bbox = this.stack.getBoundingClientRect();

      this.customPreset.width = bbox.width - 40; // horizontal padding of the container
      this.customPreset.height = bbox.height - 80; // vertical padding + toolbar height

      this.currentPresetKey = this.presets[1].key; // most common preset
    }
  },

  /**
   * Destroy the nodes. Remove listeners. Reset the style.
   */
  close: Task.async(function* () {
    debug("CLOSE BEGINS");
    if (this.closing) {
      debug("ALREADY CLOSING, ABORT");
      return;
    }
    this.closing = true;

    // If we're closing very fast (in tests), ensure init has finished.
    debug("CLOSE: WAIT ON INITED");
    yield this.inited;
    debug("CLOSE: INITED DONE");

    this.unCheckMenus();
    // Reset style of the stack.
    debug(`CURRENT SIZE: ${this.stack.getAttribute("style")}`);
    let style = "max-width: none;" +
                "min-width: 0;" +
                "max-height: none;" +
                "min-height: 0;";
    debug("RESET STACK SIZE");
    this.stack.setAttribute("style", style);

    // Wait for resize message before stopping in the child when testing,
    // but only if we should expect to still get a message.
    if (flags.testing && this.tab.linkedBrowser.messageManager) {
      debug("CLOSE: WAIT ON CONTENT RESIZE");
      yield this.waitForMessage("ResponsiveMode:OnContentResize");
      debug("CLOSE: CONTENT RESIZE DONE");
    }

    if (this.isResizing) {
      this.stopResizing();
    }

    // Remove listeners.
    this.menulist.removeEventListener("select", this.boundPresetSelected, true);
    this.menulist.removeEventListener("change", this.boundHandleManualInput, true);
    this.mainWindow.removeEventListener("unload", this);
    this.tab.removeEventListener("TabClose", this);
    this.tabContainer.removeEventListener("TabSelect", this);
    this.rotatebutton.removeEventListener("command", this.boundRotate, true);
    this.screenshotbutton.removeEventListener("command", this.boundScreenshot, true);
    this.closebutton.removeEventListener("command", this.boundClose, true);
    this.addbutton.removeEventListener("command", this.boundAddPreset, true);
    this.removebutton.removeEventListener("command", this.boundRemovePreset, true);
    this.touchbutton.removeEventListener("command", this.boundTouch, true);
    this.userAgentInput.removeEventListener("blur", this.boundChangeUA, true);

    // Removed elements.
    this.container.removeChild(this.toolbar);
    if (this.bottomToolbar) {
      this.bottomToolbar.remove();
      delete this.bottomToolbar;
    }
    this.stack.removeChild(this.resizer);
    this.stack.removeChild(this.resizeBarV);
    this.stack.removeChild(this.resizeBarH);

    this.stack.classList.remove("fxos-mode");

    // Unset the responsive mode.
    this.container.removeAttribute("responsivemode");
    this.stack.removeAttribute("responsivemode");

    ActiveTabs.delete(this.tab);
    if (this.touchEventSimulator) {
      this.touchEventSimulator.stop();
    }

    debug("CLOSE: WAIT ON CLIENT CLOSE");
    yield this.client.close();
    debug("CLOSE: CLIENT CLOSE DONE");
    this.client = this.emulationFront = null;

    this._telemetry.toolClosed("responsive");

    if (this.tab.linkedBrowser && this.tab.linkedBrowser.messageManager) {
      let stopped = this.waitForMessage("ResponsiveMode:Stop:Done");
      this.tab.linkedBrowser.messageManager.sendAsyncMessage("ResponsiveMode:Stop");
      debug("CLOSE: WAIT ON STOP");
      yield stopped;
      debug("CLOSE: STOP DONE");
    }

    this.hideNewUINotification();

    debug("CLOSE: DONE, EMIT OFF");
    this.inited = null;
    ResponsiveUIManager.emit("off", { tab: this.tab });
  }),

  waitForMessage(message) {
    return new Promise(resolve => {
      let listener = () => {
        this.mm.removeMessageListener(message, listener);
        resolve();
      };
      this.mm.addMessageListener(message, listener);
    });
  },

  /**
   * Emit an event when the content has been resized. Only used in tests.
   */
  onContentResize: function (msg) {
    ResponsiveUIManager.emit("content-resize", {
      tab: this.tab,
      width: msg.data.width,
      height: msg.data.height,
    });
  },

  /**
   * Handle events
   */
  handleEvent: function (event) {
    switch (event.type) {
      case "TabClose":
      case "unload":
        this.close();
        break;
      case "TabSelect":
        if (this.tab.selected) {
          this.checkMenus();
        } else if (!this.mainWindow.gBrowser.selectedTab.responsiveUI) {
          this.unCheckMenus();
        }
        break;
    }
  },

  getViewportBrowser() {
    return this.browser;
  },

  /**
   * Check the menu items.
   */
  checkMenus: function () {
    this.chromeDoc.getElementById("menu_responsiveUI").setAttribute("checked", "true");
  },

  /**
   * Uncheck the menu items.
   */
  unCheckMenus: function () {
    let el = this.chromeDoc.getElementById("menu_responsiveUI");
    if (el) {
      el.setAttribute("checked", "false");
    }
  },

  /**
   * Build the toolbar and the resizers.
   *
   * <vbox class="browserContainer"> From tabbrowser.xml
   *  <toolbar class="devtools-responsiveui-toolbar">
   *    <menulist class="devtools-responsiveui-menulist"/> // presets
   *    <toolbarbutton tabindex="0" class="devtools-responsiveui-toolbarbutton"
   *                   tooltiptext="rotate"/> // rotate
   *    <toolbarbutton tabindex="0" class="devtools-responsiveui-toolbarbutton"
   *                   tooltiptext="screenshot"/> // screenshot
   *    <toolbarbutton tabindex="0" class="devtools-responsiveui-toolbarbutton"
   *                   tooltiptext="Leave Responsive Design Mode"/> // close
   *  </toolbar>
   *  <stack class="browserStack"> From tabbrowser.xml
   *    <browser/>
   *    <box class="devtools-responsiveui-resizehandle" bottom="0" right="0"/>
   *    <box class="devtools-responsiveui-resizebarV" top="0" right="0"/>
   *    <box class="devtools-responsiveui-resizebarH" bottom="0" left="0"/>
   *    // Additional button in FxOS mode:
   *    <button class="devtools-responsiveui-sleep-button" />
   *    <vbox class="devtools-responsiveui-volume-buttons">
   *      <button class="devtools-responsiveui-volume-up-button" />
   *      <button class="devtools-responsiveui-volume-down-button" />
   *    </vbox>
   *  </stack>
   *  <toolbar class="devtools-responsiveui-hardware-button">
   *    <toolbarbutton class="devtools-responsiveui-home-button" />
   *  </toolbar>
   * </vbox>
   */
  buildUI: function () {
    // Toolbar
    this.toolbar = this.chromeDoc.createElement("toolbar");
    this.toolbar.className = "devtools-responsiveui-toolbar";
    this.toolbar.setAttribute("fullscreentoolbar", "true");

    this.menulist = this.chromeDoc.createElement("menulist");
    this.menulist.className = "devtools-responsiveui-menulist";
    this.menulist.setAttribute("editable", "true");

    this.menulist.addEventListener("select", this.boundPresetSelected, true);
    this.menulist.addEventListener("change", this.boundHandleManualInput, true);

    this.menuitems = new Map();

    let menupopup = this.chromeDoc.createElement("menupopup");
    this.registerPresets(menupopup);
    this.menulist.appendChild(menupopup);

    this.addbutton = this.chromeDoc.createElement("menuitem");
    this.addbutton.setAttribute(
      "label",
      this.strings.GetStringFromName("responsiveUI.addPreset")
    );
    this.addbutton.addEventListener("command", this.boundAddPreset, true);

    this.removebutton = this.chromeDoc.createElement("menuitem");
    this.removebutton.setAttribute(
      "label",
      this.strings.GetStringFromName("responsiveUI.removePreset")
    );
    this.removebutton.addEventListener("command", this.boundRemovePreset, true);

    menupopup.appendChild(this.chromeDoc.createElement("menuseparator"));
    menupopup.appendChild(this.addbutton);
    menupopup.appendChild(this.removebutton);

    this.rotatebutton = this.chromeDoc.createElement("toolbarbutton");
    this.rotatebutton.setAttribute("tabindex", "0");
    this.rotatebutton.setAttribute(
      "tooltiptext",
      this.strings.GetStringFromName("responsiveUI.rotate2")
    );
    this.rotatebutton.className =
      "devtools-responsiveui-toolbarbutton devtools-responsiveui-rotate";
    this.rotatebutton.addEventListener("command", this.boundRotate, true);

    this.screenshotbutton = this.chromeDoc.createElement("toolbarbutton");
    this.screenshotbutton.setAttribute("tabindex", "0");
    this.screenshotbutton.setAttribute(
      "tooltiptext",
      this.strings.GetStringFromName("responsiveUI.screenshot")
    );
    this.screenshotbutton.className =
      "devtools-responsiveui-toolbarbutton devtools-responsiveui-screenshot";
    this.screenshotbutton.addEventListener("command", this.boundScreenshot, true);

    this.closebutton = this.chromeDoc.createElement("toolbarbutton");
    this.closebutton.setAttribute("tabindex", "0");
    this.closebutton.className =
      "devtools-responsiveui-toolbarbutton devtools-responsiveui-close";
    this.closebutton.setAttribute(
      "tooltiptext",
      this.strings.GetStringFromName("responsiveUI.close1")
    );
    this.closebutton.addEventListener("command", this.boundClose, true);

    this.toolbar.appendChild(this.closebutton);
    this.toolbar.appendChild(this.menulist);
    this.toolbar.appendChild(this.rotatebutton);

    this.touchbutton = this.chromeDoc.createElement("toolbarbutton");
    this.touchbutton.setAttribute("tabindex", "0");
    this.touchbutton.setAttribute(
      "tooltiptext",
      this.strings.GetStringFromName("responsiveUI.touch")
    );
    this.touchbutton.className =
      "devtools-responsiveui-toolbarbutton devtools-responsiveui-touch";
    this.touchbutton.addEventListener("command", this.boundTouch, true);
    this.toolbar.appendChild(this.touchbutton);

    this.toolbar.appendChild(this.screenshotbutton);

    this.userAgentInput = this.chromeDoc.createElement("textbox");
    this.userAgentInput.className = "devtools-responsiveui-textinput";
    this.userAgentInput.setAttribute("placeholder",
      this.strings.GetStringFromName("responsiveUI.userAgentPlaceholder"));
    this.userAgentInput.addEventListener("blur", this.boundChangeUA, true);
    this.userAgentInput.hidden = true;
    this.toolbar.appendChild(this.userAgentInput);

    // Resizers
    let resizerTooltip = this.strings.GetStringFromName("responsiveUI.resizerTooltip");
    this.resizer = this.chromeDoc.createElement("box");
    this.resizer.className = "devtools-responsiveui-resizehandle";
    this.resizer.setAttribute("right", "0");
    this.resizer.setAttribute("bottom", "0");
    this.resizer.setAttribute("tooltiptext", resizerTooltip);
    this.resizer.onmousedown = this.boundStartResizing;

    this.resizeBarV = this.chromeDoc.createElement("box");
    this.resizeBarV.className = "devtools-responsiveui-resizebarV";
    this.resizeBarV.setAttribute("top", "0");
    this.resizeBarV.setAttribute("right", "0");
    this.resizeBarV.setAttribute("tooltiptext", resizerTooltip);
    this.resizeBarV.onmousedown = this.boundStartResizing;

    this.resizeBarH = this.chromeDoc.createElement("box");
    this.resizeBarH.className = "devtools-responsiveui-resizebarH";
    this.resizeBarH.setAttribute("bottom", "0");
    this.resizeBarH.setAttribute("left", "0");
    this.resizeBarH.setAttribute("tooltiptext", resizerTooltip);
    this.resizeBarH.onmousedown = this.boundStartResizing;

    this.container.insertBefore(this.toolbar, this.stack);
    this.stack.appendChild(this.resizer);
    this.stack.appendChild(this.resizeBarV);
    this.stack.appendChild(this.resizeBarH);
  },

  // FxOS custom controls
  buildPhoneUI: function () {
    this.stack.classList.add("fxos-mode");

    let sleepButton = this.chromeDoc.createElement("button");
    sleepButton.className = "devtools-responsiveui-sleep-button";
    sleepButton.setAttribute("top", 0);
    sleepButton.setAttribute("right", 0);
    sleepButton.addEventListener("mousedown", () => {
      SystemAppProxy.dispatchKeyboardEvent("keydown", { key: "Power" });
    });
    sleepButton.addEventListener("mouseup", () => {
      SystemAppProxy.dispatchKeyboardEvent("keyup", { key: "Power" });
    });
    this.stack.appendChild(sleepButton);

    let volumeButtons = this.chromeDoc.createElement("vbox");
    volumeButtons.className = "devtools-responsiveui-volume-buttons";
    volumeButtons.setAttribute("top", 0);
    volumeButtons.setAttribute("left", 0);

    let volumeUp = this.chromeDoc.createElement("button");
    volumeUp.className = "devtools-responsiveui-volume-up-button";
    volumeUp.addEventListener("mousedown", () => {
      SystemAppProxy.dispatchKeyboardEvent("keydown", { key: "AudioVolumeUp" });
    });
    volumeUp.addEventListener("mouseup", () => {
      SystemAppProxy.dispatchKeyboardEvent("keyup", { key: "AudioVolumeUp" });
    });

    let volumeDown = this.chromeDoc.createElement("button");
    volumeDown.className = "devtools-responsiveui-volume-down-button";
    volumeDown.addEventListener("mousedown", () => {
      SystemAppProxy.dispatchKeyboardEvent("keydown", { key: "AudioVolumeDown" });
    });
    volumeDown.addEventListener("mouseup", () => {
      SystemAppProxy.dispatchKeyboardEvent("keyup", { key: "AudioVolumeDown" });
    });

    volumeButtons.appendChild(volumeUp);
    volumeButtons.appendChild(volumeDown);
    this.stack.appendChild(volumeButtons);

    let bottomToolbar = this.chromeDoc.createElement("toolbar");
    bottomToolbar.className = "devtools-responsiveui-hardware-buttons";
    bottomToolbar.setAttribute("align", "center");
    bottomToolbar.setAttribute("pack", "center");

    let homeButton = this.chromeDoc.createElement("toolbarbutton");
    homeButton.className =
      "devtools-responsiveui-toolbarbutton devtools-responsiveui-home-button";
    homeButton.addEventListener("mousedown", () => {
      SystemAppProxy.dispatchKeyboardEvent("keydown", { key: "Home" });
    });
    homeButton.addEventListener("mouseup", () => {
      SystemAppProxy.dispatchKeyboardEvent("keyup", { key: "Home" });
    });
    bottomToolbar.appendChild(homeButton);
    this.bottomToolbar = bottomToolbar;
    this.container.appendChild(bottomToolbar);
  },

  showNewUINotification() {
    let nbox = this.mainWindow.gBrowser.getNotificationBox(this.browser);

    // One reason we might be using old RDM is that the user explcitly disabled new RDM.
    // We should encourage them to use the new one, since the old one will be removed.
    if (Services.prefs.prefHasUserValue(NEW_RDM_ENABLED) &&
        !Services.prefs.getBoolPref(NEW_RDM_ENABLED)) {
      let buttons = [{
        label: this.strings.GetStringFromName("responsiveUI.newVersionEnableAndRestart"),
        callback: () => {
          Services.prefs.setBoolPref(NEW_RDM_ENABLED, true);
          BrowserUtils.restartApplication();
        },
      }];
      nbox.appendNotification(
        this.strings.GetStringFromName("responsiveUI.newVersionUserDisabled"),
        "responsive-ui-new-version-user-disabled",
        null,
        nbox.PRIORITY_INFO_LOW,
        buttons
      );
      return;
    }

    // Only show a notification about the new RDM UI on channels where there is an e10s
    // switch in the preferences UI (Dev. Ed, Nightly).  On other channels, it is less
    // clear how a user would proceed here, so don't show a message.
    if (!system.constants.E10S_TESTING_ONLY) {
      return;
    }

    let buttons = [{
      label: this.strings.GetStringFromName("responsiveUI.newVersionEnableAndRestart"),
      callback: () => {
        Services.prefs.setBoolPref("browser.tabs.remote.autostart", true);
        Services.prefs.setBoolPref("browser.tabs.remote.autostart.2", true);
        BrowserUtils.restartApplication();
      },
    }];
    nbox.appendNotification(
      this.strings.GetStringFromName("responsiveUI.newVersionE10sDisabled"),
      "responsive-ui-new-version-e10s-disabled",
      null,
      nbox.PRIORITY_INFO_LOW,
      buttons
    );
  },

  hideNewUINotification() {
    if (!this.mainWindow.gBrowser || !this.mainWindow.gBrowser.getNotificationBox) {
      return;
    }
    let nbox = this.mainWindow.gBrowser.getNotificationBox(this.browser);
    let n = nbox.getNotificationWithValue("responsive-ui-new-version-user-disabled");
    if (n) {
      n.close();
    }
    n = nbox.getNotificationWithValue("responsive-ui-new-version-e10s-disabled");
    if (n) {
      n.close();
    }
  },

  /**
   * Validate and apply any user input on the editable menulist
   */
  handleManualInput: function () {
    let userInput = this.menulist.inputField.value;
    let value = INPUT_PARSER.exec(userInput);
    let selectedPreset = this.menuitems.get(this.selectedItem);

    // In case of an invalide value, we show back the last preset
    if (!value || value.length < 3) {
      this.setMenuLabel(this.selectedItem, selectedPreset);
      return;
    }

    this.rotateValue = false;

    if (!selectedPreset.custom) {
      let menuitem = this.customMenuitem;
      this.currentPresetKey = this.customPreset.key;
      this.menulist.selectedItem = menuitem;
    }

    let w = this.customPreset.width = parseInt(value[1], 10);
    let h = this.customPreset.height = parseInt(value[2], 10);

    this.saveCustomSize();
    this.setViewportSize({
      width: w,
      height: h,
    });
  },

  /**
   * Build the presets list and append it to the menupopup.
   *
   * @param parent menupopup.
   */
  registerPresets: function (parent) {
    let fragment = this.chromeDoc.createDocumentFragment();
    let doc = this.chromeDoc;

    for (let preset of this.presets) {
      let menuitem = doc.createElement("menuitem");
      menuitem.setAttribute("ispreset", true);
      this.menuitems.set(menuitem, preset);

      if (preset.key === this.currentPresetKey) {
        menuitem.setAttribute("selected", "true");
        this.selectedItem = menuitem;
      }

      if (preset.custom) {
        this.customMenuitem = menuitem;
      }

      this.setMenuLabel(menuitem, preset);
      fragment.appendChild(menuitem);
    }
    parent.appendChild(fragment);
  },

  /**
   * Set the menuitem label of a preset.
   *
   * @param menuitem menuitem to edit.
   * @param preset associated preset.
   */
  setMenuLabel: function (menuitem, preset) {
    let size = SHARED_L10N.getFormatStr("dimensions",
      Math.round(preset.width), Math.round(preset.height));

    // .inputField might be not reachable yet (async XBL loading)
    if (this.menulist.inputField) {
      this.menulist.inputField.value = size;
    }

    if (preset.custom) {
      size = this.strings.formatStringFromName("responsiveUI.customResolution",
                                               [size], 1);
    } else if (preset.name != null && preset.name !== "") {
      size = this.strings.formatStringFromName("responsiveUI.namedResolution",
                                               [size, preset.name], 2);
    }

    menuitem.setAttribute("label", size);
  },

  /**
   * When a preset is selected, apply it.
   */
  presetSelected: function () {
    if (this.menulist.selectedItem.getAttribute("ispreset") === "true") {
      this.selectedItem = this.menulist.selectedItem;

      this.rotateValue = false;
      let selectedPreset = this.menuitems.get(this.selectedItem);
      this.loadPreset(selectedPreset);
      this.currentPresetKey = selectedPreset.key;
      this.saveCurrentPreset();

      // Update the buttons hidden status according to the new selected preset
      if (selectedPreset == this.customPreset) {
        this.addbutton.hidden = false;
        this.removebutton.hidden = true;
      } else {
        this.addbutton.hidden = true;
        this.removebutton.hidden = false;
      }
    }
  },

  /**
   * Apply a preset.
   */
  loadPreset(preset) {
    this.setViewportSize(preset);
  },

  /**
   * Add a preset to the list and the memory
   */
  addPreset: function () {
    let w = this.customPreset.width;
    let h = this.customPreset.height;
    let newName = {};

    let title = this.strings.GetStringFromName("responsiveUI.customNamePromptTitle1");
    let message = this.strings.formatStringFromName("responsiveUI.customNamePromptMsg",
                                                    [w, h], 2);
    let promptOk = Services.prompt.prompt(null, title, message, newName, null, {});

    if (!promptOk) {
      // Prompt has been cancelled
      this.menulist.selectedItem = this.selectedItem;
      return;
    }

    let newPreset = {
      key: w + "x" + h,
      name: newName.value,
      width: w,
      height: h
    };

    this.presets.push(newPreset);

    // Sort the presets according to width/height ascending order
    this.presets.sort((presetA, presetB) => {
      // We keep custom preset at first
      if (presetA.custom && !presetB.custom) {
        return 1;
      }
      if (!presetA.custom && presetB.custom) {
        return -1;
      }

      if (presetA.width === presetB.width) {
        if (presetA.height === presetB.height) {
          return 0;
        }
        return presetA.height > presetB.height;
      }
      return presetA.width > presetB.width;
    });

    this.savePresets();

    let newMenuitem = this.chromeDoc.createElement("menuitem");
    newMenuitem.setAttribute("ispreset", true);
    this.setMenuLabel(newMenuitem, newPreset);

    this.menuitems.set(newMenuitem, newPreset);
    let idx = this.presets.indexOf(newPreset);
    let beforeMenuitem = this.menulist.firstChild.childNodes[idx + 1];
    this.menulist.firstChild.insertBefore(newMenuitem, beforeMenuitem);

    this.menulist.selectedItem = newMenuitem;
    this.currentPresetKey = newPreset.key;
    this.saveCurrentPreset();
  },

  /**
   * remove a preset from the list and the memory
   */
  removePreset: function () {
    let selectedPreset = this.menuitems.get(this.selectedItem);
    let w = selectedPreset.width;
    let h = selectedPreset.height;

    this.presets.splice(this.presets.indexOf(selectedPreset), 1);
    this.menulist.firstChild.removeChild(this.selectedItem);
    this.menuitems.delete(this.selectedItem);

    this.customPreset.width = w;
    this.customPreset.height = h;
    let menuitem = this.customMenuitem;
    this.setMenuLabel(menuitem, this.customPreset);
    this.menulist.selectedItem = menuitem;
    this.currentPresetKey = this.customPreset.key;

    this.setViewportSize({
      width: w,
      height: h,
    });

    this.savePresets();
  },

  /**
   * Swap width and height.
   */
  rotate: function () {
    let selectedPreset = this.menuitems.get(this.selectedItem);
    let width = this.rotateValue ? selectedPreset.height : selectedPreset.width;
    let height = this.rotateValue ? selectedPreset.width : selectedPreset.height;

    this.setViewportSize({
      width: height,
      height: width,
    });

    if (selectedPreset.custom) {
      this.saveCustomSize();
    } else {
      this.rotateValue = !this.rotateValue;
      this.saveCurrentPreset();
    }
  },

  /**
   * Take a screenshot of the page.
   *
   * @param filename name of the screenshot file (used for tests).
   */
  screenshot: function (filename) {
    if (!filename) {
      let date = new Date();
      let month = ("0" + (date.getMonth() + 1)).substr(-2, 2);
      let day = ("0" + date.getDate()).substr(-2, 2);
      let dateString = [date.getFullYear(), month, day].join("-");
      let timeString = date.toTimeString().replace(/:/g, ".").split(" ")[0];
      filename =
        this.strings.formatStringFromName("responsiveUI.screenshotGeneratedFilename",
                                          [dateString, timeString], 2);
    }
    let mm = this.tab.linkedBrowser.messageManager;
    let chromeWindow = this.chromeDoc.defaultView;
    let doc = chromeWindow.document;
    function onScreenshot(message) {
      mm.removeMessageListener("ResponsiveMode:RequestScreenshot:Done", onScreenshot);
      chromeWindow.saveURL(message.data, filename + ".png", null, true, true,
                           doc.documentURIObject, doc);
    }
    mm.addMessageListener("ResponsiveMode:RequestScreenshot:Done", onScreenshot);
    mm.sendAsyncMessage("ResponsiveMode:RequestScreenshot");
  },

  /**
   * Enable/Disable mouse -> touch events translation.
   */
  enableTouch: function () {
    this.touchbutton.setAttribute("checked", "true");
    return this.touchEventSimulator.start();
  },

  disableTouch: function () {
    this.touchbutton.removeAttribute("checked");
    return this.touchEventSimulator.stop();
  },

  hideTouchNotification: function () {
    let nbox = this.mainWindow.gBrowser.getNotificationBox(this.browser);
    let n = nbox.getNotificationWithValue("responsive-ui-need-reload");
    if (n) {
      n.close();
    }
  },

  toggleTouch: Task.async(function* () {
    this.hideTouchNotification();
    if (this.touchEventSimulator.enabled) {
      this.disableTouch();
      return;
    }

    let isReloadNeeded = yield this.enableTouch();
    if (!isReloadNeeded) {
      return;
    }

    const PREF = "devtools.responsiveUI.no-reload-notification";
    if (Services.prefs.getBoolPref(PREF)) {
      return;
    }

    let nbox = this.mainWindow.gBrowser.getNotificationBox(this.browser);

    let buttons = [{
      label: this.strings.GetStringFromName("responsiveUI.notificationReload"),
      callback: () => this.browser.reload(),
      accessKey:
        this.strings.GetStringFromName("responsiveUI.notificationReload_accesskey"),
    }, {
      label: this.strings.GetStringFromName("responsiveUI.dontShowReloadNotification"),
      callback: () => Services.prefs.setBoolPref(PREF, true),
      accessKey:
        this.strings
            .GetStringFromName("responsiveUI.dontShowReloadNotification_accesskey"),
    }];

    nbox.appendNotification(
      this.strings.GetStringFromName("responsiveUI.needReload"),
      "responsive-ui-need-reload",
      null,
      nbox.PRIORITY_INFO_LOW,
      buttons
    );
  }),

  waitForReload() {
    return new Promise(resolve => {
      let onNavigated = (_, { state }) => {
        if (state != "stop") {
          return;
        }
        this.client.removeListener("tabNavigated", onNavigated);
        resolve();
      };
      this.client.addListener("tabNavigated", onNavigated);
    });
  },

  /**
   * Change the user agent string
   */
  changeUA: Task.async(function* () {
    let value = this.userAgentInput.value;
    let changed;
    if (value) {
      changed = yield this.emulationFront.setUserAgentOverride(value);
      this.userAgentInput.setAttribute("attention", "true");
    } else {
      changed = yield this.emulationFront.clearUserAgentOverride();
      this.userAgentInput.removeAttribute("attention");
    }
    if (changed) {
      let reloaded = this.waitForReload();
      this.tab.linkedBrowser.reload();
      yield reloaded;
    }
    ResponsiveUIManager.emit("userAgentChanged", { tab: this.tab });
  }),

  /**
   * Get the current width and height.
   */
  getSize() {
    let width = Number(this.stack.style.minWidth.replace("px", ""));
    let height = Number(this.stack.style.minHeight.replace("px", ""));
    return {
      width,
      height,
    };
  },

  /**
   * Change the size of the viewport.
   */
  setViewportSize({ width, height }) {
    debug(`SET SIZE TO ${width} x ${height}`);
    if (this.closing) {
      debug(`ABORT SET SIZE, CLOSING`);
      return;
    }
    if (width) {
      this.setWidth(width);
    }
    if (height) {
      this.setHeight(height);
    }
  },

  setWidth: function (width) {
    width = Math.min(Math.max(width, MIN_WIDTH), MAX_WIDTH);
    this.stack.style.maxWidth = this.stack.style.minWidth = width + "px";

    if (!this.ignoreX) {
      this.resizeBarH.setAttribute("left", Math.round(width / 2));
    }

    let selectedPreset = this.menuitems.get(this.selectedItem);

    if (selectedPreset.custom) {
      selectedPreset.width = width;
      this.setMenuLabel(this.selectedItem, selectedPreset);
    }
  },

  setHeight: function (height) {
    height = Math.min(Math.max(height, MIN_HEIGHT), MAX_HEIGHT);
    this.stack.style.maxHeight = this.stack.style.minHeight = height + "px";

    if (!this.ignoreY) {
      this.resizeBarV.setAttribute("top", Math.round(height / 2));
    }

    let selectedPreset = this.menuitems.get(this.selectedItem);
    if (selectedPreset.custom) {
      selectedPreset.height = height;
      this.setMenuLabel(this.selectedItem, selectedPreset);
    }
  },
  /**
   * Start the process of resizing the browser.
   *
   * @param event
   */
  startResizing: function (event) {
    let selectedPreset = this.menuitems.get(this.selectedItem);

    if (!selectedPreset.custom) {
      if (this.rotateValue) {
        this.customPreset.width = selectedPreset.height;
        this.customPreset.height = selectedPreset.width;
      } else {
        this.customPreset.width = selectedPreset.width;
        this.customPreset.height = selectedPreset.height;
      }

      let menuitem = this.customMenuitem;
      this.setMenuLabel(menuitem, this.customPreset);

      this.currentPresetKey = this.customPreset.key;
      this.menulist.selectedItem = menuitem;
    }
    this.mainWindow.addEventListener("mouseup", this.boundStopResizing, true);
    this.mainWindow.addEventListener("mousemove", this.boundOnDrag, true);
    this.container.style.pointerEvents = "none";

    this._resizing = true;
    this.stack.setAttribute("notransition", "true");

    this.lastScreenX = event.screenX;
    this.lastScreenY = event.screenY;

    this.ignoreY = (event.target === this.resizeBarV);
    this.ignoreX = (event.target === this.resizeBarH);

    this.isResizing = true;
  },

  /**
   * Resizing on mouse move.
   *
   * @param event
   */
  onDrag: function (event) {
    let shift = event.shiftKey;
    let ctrl = !event.shiftKey && event.ctrlKey;

    let screenX = event.screenX;
    let screenY = event.screenY;

    let deltaX = screenX - this.lastScreenX;
    let deltaY = screenY - this.lastScreenY;

    if (this.ignoreY) {
      deltaY = 0;
    }
    if (this.ignoreX) {
      deltaX = 0;
    }

    if (ctrl) {
      deltaX /= SLOW_RATIO;
      deltaY /= SLOW_RATIO;
    }

    let width = this.customPreset.width + deltaX;
    let height = this.customPreset.height + deltaY;

    if (shift) {
      let roundedWidth, roundedHeight;
      roundedWidth = 10 * Math.floor(width / ROUND_RATIO);
      roundedHeight = 10 * Math.floor(height / ROUND_RATIO);
      screenX += roundedWidth - width;
      screenY += roundedHeight - height;
      width = roundedWidth;
      height = roundedHeight;
    }

    if (width < MIN_WIDTH) {
      width = MIN_WIDTH;
    } else {
      this.lastScreenX = screenX;
    }

    if (height < MIN_HEIGHT) {
      height = MIN_HEIGHT;
    } else {
      this.lastScreenY = screenY;
    }

    this.setViewportSize({ width, height });
  },

  /**
   * Stop End resizing
   */
  stopResizing: function () {
    this.container.style.pointerEvents = "auto";

    this.mainWindow.removeEventListener("mouseup", this.boundStopResizing, true);
    this.mainWindow.removeEventListener("mousemove", this.boundOnDrag, true);

    this.saveCustomSize();

    delete this._resizing;
    if (this.transitionsEnabled) {
      this.stack.removeAttribute("notransition");
    }
    this.ignoreY = false;
    this.ignoreX = false;
    this.isResizing = false;
  },

  /**
   * Store the custom size as a pref.
   */
  saveCustomSize: function () {
    Services.prefs.setIntPref("devtools.responsiveUI.customWidth",
                              this.customPreset.width);
    Services.prefs.setIntPref("devtools.responsiveUI.customHeight",
                              this.customPreset.height);
  },

  /**
   * Store the current preset as a pref.
   */
  saveCurrentPreset: function () {
    Services.prefs.setCharPref("devtools.responsiveUI.currentPreset",
                               this.currentPresetKey);
    Services.prefs.setBoolPref("devtools.responsiveUI.rotate",
                               this.rotateValue);
  },

  /**
   * Store the list of all registered presets as a pref.
   */
  savePresets: function () {
    // We exclude the custom one
    let registeredPresets = this.presets.filter(function (preset) {
      return !preset.custom;
    });
    Services.prefs.setCharPref("devtools.responsiveUI.presets",
                               JSON.stringify(registeredPresets));
  },
};

loader.lazyGetter(ResponsiveUI.prototype, "strings", function () {
  return Services.strings.createBundle("chrome://devtools/locale/responsiveUI.properties");
});
