/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/devtools/gDevTools.jsm");
Cu.import("resource://gre/modules/devtools/event-emitter.js");
Cu.import("resource:///modules/devtools/ViewHelpers.jsm");
let { Promise: promise } = Cu.import("resource://gre/modules/Promise.jsm", {});
XPCOMUtils.defineLazyModuleGetter(this, "SystemAppProxy",
                                  "resource://gre/modules/SystemAppProxy.jsm");

var require = Cu.import("resource://gre/modules/devtools/Loader.jsm", {}).devtools.require;
let Telemetry = require("devtools/shared/telemetry");
let { showDoorhanger } = require("devtools/shared/doorhanger");
let { TouchEventSimulator } = require("devtools/toolkit/touch/simulator");
let { Task } = require("resource://gre/modules/Task.jsm");

this.EXPORTED_SYMBOLS = ["ResponsiveUIManager"];

const MIN_WIDTH = 50;
const MIN_HEIGHT = 50;

const MAX_WIDTH = 10000;
const MAX_HEIGHT = 10000;

const SLOW_RATIO = 6;
const ROUND_RATIO = 10;

const INPUT_PARSER = /(\d+)[^\d]+(\d+)/;

const SHARED_L10N = new ViewHelpers.L10N("chrome://browser/locale/devtools/shared.properties");

let ActiveTabs = new Map();

this.ResponsiveUIManager = {
  /**
   * Check if the a tab is in a responsive mode.
   * Leave the responsive mode if active,
   * active the responsive mode if not active.
   *
   * @param aWindow the main window.
   * @param aTab the tab targeted.
   */
  toggle: function(aWindow, aTab) {
    if (this.isActiveForTab(aTab)) {
      ActiveTabs.get(aTab).close();
    } else {
      new ResponsiveUI(aWindow, aTab);
    }
  },

  /**
   * Returns true if responsive view is active for the provided tab.
   *
   * @param aTab the tab targeted.
   */
  isActiveForTab: function(aTab) {
    return ActiveTabs.has(aTab);
  },

  /**
   * Return the responsive UI controller for a tab.
   */
  getResponsiveUIForTab: function(aTab) {
    return ActiveTabs.get(aTab);
  },

  /**
   * Handle gcli commands.
   *
   * @param aWindow the browser window.
   * @param aTab the tab targeted.
   * @param aCommand the command name.
   * @param aArgs command arguments.
   */
  handleGcliCommand: function(aWindow, aTab, aCommand, aArgs) {
    switch (aCommand) {
      case "resize to":
        if (!this.isActiveForTab(aTab)) {
          new ResponsiveUI(aWindow, aTab);
        }
        ActiveTabs.get(aTab).setSize(aArgs.width, aArgs.height);
        break;
      case "resize on":
        if (!this.isActiveForTab(aTab)) {
          new ResponsiveUI(aWindow, aTab);
        }
        break;
      case "resize off":
        if (this.isActiveForTab(aTab)) {
          ActiveTabs.get(aTab).close();
        }
        break;
      case "resize toggle":
          this.toggle(aWindow, aTab);
      default:
    }
  }
}

EventEmitter.decorate(ResponsiveUIManager);

let presets = [
  // Phones
  {key: "320x480", width: 320, height: 480},    // iPhone, B2G, with <meta viewport>
  {key: "360x640", width: 360, height: 640},    // Android 4, phones, with <meta viewport>

  // Tablets
  {key: "768x1024", width: 768, height: 1024},   // iPad, with <meta viewport>
  {key: "800x1280", width: 800, height: 1280},   // Android 4, Tablet, with <meta viewport>

  // Default width for mobile browsers, no <meta viewport>
  {key: "980x1280", width: 980, height: 1280},

  // Computer
  {key: "1280x600", width: 1280, height: 600},
  {key: "1920x900", width: 1920, height: 900},
];

function ResponsiveUI(aWindow, aTab)
{
  this.mainWindow = aWindow;
  this.tab = aTab;
  this.mm = this.tab.linkedBrowser.messageManager;
  this.tabContainer = aWindow.gBrowser.tabContainer;
  this.browser = aTab.linkedBrowser;
  this.chromeDoc = aWindow.document;
  this.container = aWindow.gBrowser.getBrowserContainer(this.browser);
  this.stack = this.container.querySelector(".browserStack");
  this._telemetry = new Telemetry();

  let childOn = () => {
    this.mm.removeMessageListener("ResponsiveMode:Start:Done", childOn);
    ResponsiveUIManager.emit("on", { tab: this.tab });
  }
  this.mm.addMessageListener("ResponsiveMode:Start:Done", childOn);

  let requiresFloatingScrollbars = !this.mainWindow.matchMedia("(-moz-overlay-scrollbars)").matches;
  this.mm.loadFrameScript("resource:///modules/devtools/responsivedesign-child.js", true);
  this.mm.addMessageListener("ResponsiveMode:ChildScriptReady", () => {
    this.mm.sendAsyncMessage("ResponsiveMode:Start", {
      requiresFloatingScrollbars: requiresFloatingScrollbars
    });
  });

  // Try to load presets from prefs
  if (Services.prefs.prefHasUserValue("devtools.responsiveUI.presets")) {
    try {
      presets = JSON.parse(Services.prefs.getCharPref("devtools.responsiveUI.presets"));
    } catch(e) {
      // User pref is malformated.
      Cu.reportError("Could not parse pref `devtools.responsiveUI.presets`: " + e);
    }
  }

  this.customPreset = {key: "custom", custom: true};

  if (Array.isArray(presets)) {
    this.presets = [this.customPreset].concat(presets);
  } else {
    Cu.reportError("Presets value (devtools.responsiveUI.presets) is malformated.");
    this.presets = [this.customPreset];
  }

  try {
    let width = Services.prefs.getIntPref("devtools.responsiveUI.customWidth");
    let height = Services.prefs.getIntPref("devtools.responsiveUI.customHeight");
    this.customPreset.width = Math.min(MAX_WIDTH, width);
    this.customPreset.height = Math.min(MAX_HEIGHT, height);

    this.currentPresetKey = Services.prefs.getCharPref("devtools.responsiveUI.currentPreset");
  } catch(e) {
    // Default size. The first preset (custom) is the one that will be used.
    let bbox = this.stack.getBoundingClientRect();

    this.customPreset.width = bbox.width - 40; // horizontal padding of the container
    this.customPreset.height = bbox.height - 80; // vertical padding + toolbar height

    this.currentPresetKey = this.presets[1].key; // most common preset
  }

  this.container.setAttribute("responsivemode", "true");
  this.stack.setAttribute("responsivemode", "true");

  // Let's bind some callbacks.
  this.bound_presetSelected = this.presetSelected.bind(this);
  this.bound_handleManualInput = this.handleManualInput.bind(this);
  this.bound_addPreset = this.addPreset.bind(this);
  this.bound_removePreset = this.removePreset.bind(this);
  this.bound_rotate = this.rotate.bind(this);
  this.bound_screenshot = () => this.screenshot();
  this.bound_touch = this.toggleTouch.bind(this);
  this.bound_close = this.close.bind(this);
  this.bound_startResizing = this.startResizing.bind(this);
  this.bound_stopResizing = this.stopResizing.bind(this);
  this.bound_onDrag = this.onDrag.bind(this);

  // Events
  this.tab.addEventListener("TabClose", this);
  this.tabContainer.addEventListener("TabSelect", this);

  this.buildUI();
  this.checkMenus();

  try {
    if (Services.prefs.getBoolPref("devtools.responsiveUI.rotate")) {
      this.rotate();
    }
  } catch(e) {}

  ActiveTabs.set(aTab, this);

  this._telemetry.toolOpened("responsive");

  // Touch events support
  this.touchEnableBefore = false;
  this.touchEventSimulator = new TouchEventSimulator(this.browser);

  // Hook to display promotional Developer Edition doorhanger. Only displayed once.
  showDoorhanger({
    window: this.mainWindow,
    type: "deveditionpromo",
    anchor: this.chromeDoc.querySelector("#content")
  });
}

ResponsiveUI.prototype = {
  _transitionsEnabled: true,
  get transitionsEnabled() this._transitionsEnabled,
  set transitionsEnabled(aValue) {
    this._transitionsEnabled = aValue;
    if (aValue && !this._resizing && this.stack.hasAttribute("responsivemode")) {
      this.stack.removeAttribute("notransition");
    } else if (!aValue) {
      this.stack.setAttribute("notransition", "true");
    }
  },

  /**
   * Destroy the nodes. Remove listeners. Reset the style.
   */
  close: function RUI_close() {
    if (this.closing)
      return;
    this.closing = true;

    this.unCheckMenus();
    // Reset style of the stack.
    let style = "max-width: none;" +
                "min-width: 0;" +
                "max-height: none;" +
                "min-height: 0;";
    this.stack.setAttribute("style", style);

    if (this.isResizing)
      this.stopResizing();

    // Remove listeners.
    this.menulist.removeEventListener("select", this.bound_presetSelected, true);
    this.menulist.removeEventListener("change", this.bound_handleManualInput, true);
    this.tab.removeEventListener("TabClose", this);
    this.tabContainer.removeEventListener("TabSelect", this);
    this.rotatebutton.removeEventListener("command", this.bound_rotate, true);
    this.screenshotbutton.removeEventListener("command", this.bound_screenshot, true);
    this.closebutton.removeEventListener("command", this.bound_close, true);
    this.addbutton.removeEventListener("command", this.bound_addPreset, true);
    this.removebutton.removeEventListener("command", this.bound_removePreset, true);
    this.touchbutton.removeEventListener("command", this.bound_touch, true);

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
    this._telemetry.toolClosed("responsive");
    let childOff = () => {
      this.mm.removeMessageListener("ResponsiveMode:Stop:Done", childOff);
      ResponsiveUIManager.emit("off", { tab: this.tab });
    }
    this.mm.addMessageListener("ResponsiveMode:Stop:Done", childOff);
    this.tab.linkedBrowser.messageManager.sendAsyncMessage("ResponsiveMode:Stop");
  },

  /**
   * Notify when the content has been resized. Only used in tests.
   */
  _test_notifyOnResize: function() {
    let deferred = promise.defer();
    let mm = this.mm;

    this.bound_onContentResize = this.onContentResize.bind(this);

    mm.addMessageListener("ResponsiveMode:OnContentResize", this.bound_onContentResize);

    mm.sendAsyncMessage("ResponsiveMode:NotifyOnResize");
    mm.addMessageListener("ResponsiveMode:NotifyOnResize:Done", function onListeningResize() {
      mm.removeMessageListener("ResponsiveMode:NotifyOnResize:Done", onListeningResize);
      deferred.resolve();
    });
    return deferred.promise;
  },

  onContentResize: function() {
    ResponsiveUIManager.emit("contentResize", { tab: this.tab });
  },

  /**
   * Handle events
   */
  handleEvent: function (aEvent) {
    switch (aEvent.type) {
      case "TabClose":
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

  /**
   * Check the menu items.
   */
   checkMenus: function RUI_checkMenus() {
     this.chromeDoc.getElementById("Tools:ResponsiveUI").setAttribute("checked", "true");
   },

  /**
   * Uncheck the menu items.
   */
   unCheckMenus: function RUI_unCheckMenus() {
     this.chromeDoc.getElementById("Tools:ResponsiveUI").setAttribute("checked", "false");
   },

  /**
   * Build the toolbar and the resizers.
   *
   * <vbox class="browserContainer"> From tabbrowser.xml
   *  <toolbar class="devtools-responsiveui-toolbar">
   *    <menulist class="devtools-responsiveui-menulist"/> // presets
   *    <toolbarbutton tabindex="0" class="devtools-responsiveui-toolbarbutton" tooltiptext="rotate"/> // rotate
   *    <toolbarbutton tabindex="0" class="devtools-responsiveui-toolbarbutton" tooltiptext="screenshot"/> // screenshot
   *    <toolbarbutton tabindex="0" class="devtools-responsiveui-toolbarbutton" tooltiptext="Leave Responsive Design View"/> // close
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
  buildUI: function RUI_buildUI() {
    // Toolbar
    this.toolbar = this.chromeDoc.createElement("toolbar");
    this.toolbar.className = "devtools-responsiveui-toolbar";

    this.menulist = this.chromeDoc.createElement("menulist");
    this.menulist.className = "devtools-responsiveui-menulist";
    this.menulist.setAttribute("editable", "true");

    this.menulist.addEventListener("select", this.bound_presetSelected, true);
    this.menulist.addEventListener("change", this.bound_handleManualInput, true);

    this.menuitems = new Map();

    let menupopup = this.chromeDoc.createElement("menupopup");
    this.registerPresets(menupopup);
    this.menulist.appendChild(menupopup);

    this.addbutton = this.chromeDoc.createElement("menuitem");
    this.addbutton.setAttribute("label", this.strings.GetStringFromName("responsiveUI.addPreset"));
    this.addbutton.addEventListener("command", this.bound_addPreset, true);

    this.removebutton = this.chromeDoc.createElement("menuitem");
    this.removebutton.setAttribute("label", this.strings.GetStringFromName("responsiveUI.removePreset"));
    this.removebutton.addEventListener("command", this.bound_removePreset, true);

    menupopup.appendChild(this.chromeDoc.createElement("menuseparator"));
    menupopup.appendChild(this.addbutton);
    menupopup.appendChild(this.removebutton);

    this.rotatebutton = this.chromeDoc.createElement("toolbarbutton");
    this.rotatebutton.setAttribute("tabindex", "0");
    this.rotatebutton.setAttribute("tooltiptext", this.strings.GetStringFromName("responsiveUI.rotate2"));
    this.rotatebutton.className = "devtools-responsiveui-toolbarbutton devtools-responsiveui-rotate";
    this.rotatebutton.addEventListener("command", this.bound_rotate, true);

    this.screenshotbutton = this.chromeDoc.createElement("toolbarbutton");
    this.screenshotbutton.setAttribute("tabindex", "0");
    this.screenshotbutton.setAttribute("tooltiptext", this.strings.GetStringFromName("responsiveUI.screenshot"));
    this.screenshotbutton.className = "devtools-responsiveui-toolbarbutton devtools-responsiveui-screenshot";
    this.screenshotbutton.addEventListener("command", this.bound_screenshot, true);

    this.closebutton = this.chromeDoc.createElement("toolbarbutton");
    this.closebutton.setAttribute("tabindex", "0");
    this.closebutton.className = "devtools-responsiveui-toolbarbutton devtools-responsiveui-close";
    this.closebutton.setAttribute("tooltiptext", this.strings.GetStringFromName("responsiveUI.close"));
    this.closebutton.addEventListener("command", this.bound_close, true);

    this.toolbar.appendChild(this.closebutton);
    this.toolbar.appendChild(this.menulist);
    this.toolbar.appendChild(this.rotatebutton);

    this.touchbutton = this.chromeDoc.createElement("toolbarbutton");
    this.touchbutton.setAttribute("tabindex", "0");
    this.touchbutton.setAttribute("tooltiptext", this.strings.GetStringFromName("responsiveUI.touch"));
    this.touchbutton.className = "devtools-responsiveui-toolbarbutton devtools-responsiveui-touch";
    this.touchbutton.addEventListener("command", this.bound_touch, true);
    this.toolbar.appendChild(this.touchbutton);

    this.toolbar.appendChild(this.screenshotbutton);

    // Resizers
    let resizerTooltip = this.strings.GetStringFromName("responsiveUI.resizerTooltip");
    this.resizer = this.chromeDoc.createElement("box");
    this.resizer.className = "devtools-responsiveui-resizehandle";
    this.resizer.setAttribute("right", "0");
    this.resizer.setAttribute("bottom", "0");
    this.resizer.setAttribute("tooltiptext", resizerTooltip);
    this.resizer.onmousedown = this.bound_startResizing;

    this.resizeBarV =  this.chromeDoc.createElement("box");
    this.resizeBarV.className = "devtools-responsiveui-resizebarV";
    this.resizeBarV.setAttribute("top", "0");
    this.resizeBarV.setAttribute("right", "0");
    this.resizeBarV.setAttribute("tooltiptext", resizerTooltip);
    this.resizeBarV.onmousedown = this.bound_startResizing;

    this.resizeBarH =  this.chromeDoc.createElement("box");
    this.resizeBarH.className = "devtools-responsiveui-resizebarH";
    this.resizeBarH.setAttribute("bottom", "0");
    this.resizeBarH.setAttribute("left", "0");
    this.resizeBarH.setAttribute("tooltiptext", resizerTooltip);
    this.resizeBarH.onmousedown = this.bound_startResizing;

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
      SystemAppProxy.dispatchKeyboardEvent("keydown", {key: "Power"});
    });
    sleepButton.addEventListener("mouseup", () => {
      SystemAppProxy.dispatchKeyboardEvent("keyup", {key: "Power"});
    });
    this.stack.appendChild(sleepButton);

    let volumeButtons = this.chromeDoc.createElement("vbox");
    volumeButtons.className = "devtools-responsiveui-volume-buttons";
    volumeButtons.setAttribute("top", 0);
    volumeButtons.setAttribute("left", 0);

    let volumeUp = this.chromeDoc.createElement("button");
    volumeUp.className = "devtools-responsiveui-volume-up-button";
    volumeUp.addEventListener("mousedown", () => {
      SystemAppProxy.dispatchKeyboardEvent("keydown", {key: "VolumeUp"});
    });
    volumeUp.addEventListener("mouseup", () => {
      SystemAppProxy.dispatchKeyboardEvent("keyup", {key: "VolumeUp"});
    });

    let volumeDown = this.chromeDoc.createElement("button");
    volumeDown.className = "devtools-responsiveui-volume-down-button";
    volumeDown.addEventListener("mousedown", () => {
      SystemAppProxy.dispatchKeyboardEvent("keydown", {key: "VolumeDown"});
    });
    volumeDown.addEventListener("mouseup", () => {
      SystemAppProxy.dispatchKeyboardEvent("keyup", {key: "VolumeDown"});
    });

    volumeButtons.appendChild(volumeUp);
    volumeButtons.appendChild(volumeDown);
    this.stack.appendChild(volumeButtons);

    let bottomToolbar = this.chromeDoc.createElement("toolbar");
    bottomToolbar.className = "devtools-responsiveui-hardware-buttons";
    bottomToolbar.setAttribute("align", "center");
    bottomToolbar.setAttribute("pack", "center");

    let homeButton = this.chromeDoc.createElement("toolbarbutton");
    homeButton.className = "devtools-responsiveui-toolbarbutton devtools-responsiveui-home-button";
    homeButton.addEventListener("mousedown", () => {
      SystemAppProxy.dispatchKeyboardEvent("keydown", {key: "Home"});
    });
    homeButton.addEventListener("mouseup", () => {
      SystemAppProxy.dispatchKeyboardEvent("keyup", {key: "Home"});
    });
    bottomToolbar.appendChild(homeButton);
    this.bottomToolbar = bottomToolbar;
    this.container.appendChild(bottomToolbar);
  },

  /**
   * Validate and apply any user input on the editable menulist
   */
  handleManualInput: function RUI_handleManualInput() {
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

    let w = this.customPreset.width = parseInt(value[1],10);
    let h = this.customPreset.height = parseInt(value[2],10);

    this.saveCustomSize();
    this.setSize(w, h);
  },

  /**
   * Build the presets list and append it to the menupopup.
   *
   * @param aParent menupopup.
   */
  registerPresets: function RUI_registerPresets(aParent) {
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
    aParent.appendChild(fragment);
  },

  /**
   * Set the menuitem label of a preset.
   *
   * @param aMenuitem menuitem to edit.
   * @param aPreset associated preset.
   */
  setMenuLabel: function RUI_setMenuLabel(aMenuitem, aPreset) {
    let size = SHARED_L10N.getFormatStr("dimensions",
      Math.round(aPreset.width), Math.round(aPreset.height));

    // .inputField might be not reachable yet (async XBL loading)
    if (this.menulist.inputField) {
      this.menulist.inputField.value = size;
    }

    if (aPreset.custom) {
      size = this.strings.formatStringFromName("responsiveUI.customResolution", [size], 1);
    } else if (aPreset.name != null && aPreset.name !== "") {
      size = this.strings.formatStringFromName("responsiveUI.namedResolution", [size, aPreset.name], 2);
    }

    aMenuitem.setAttribute("label", size);
  },

  /**
   * When a preset is selected, apply it.
   */
  presetSelected: function RUI_presetSelected() {
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
   *
   * @param aPreset preset to apply.
   */
  loadPreset: function RUI_loadPreset(aPreset) {
    this.setSize(aPreset.width, aPreset.height);
  },

  /**
   * Add a preset to the list and the memory
   */
  addPreset: function RUI_addPreset() {
    let w = this.customPreset.width;
    let h = this.customPreset.height;
    let newName = {};

    let title = this.strings.GetStringFromName("responsiveUI.customNamePromptTitle");
    let message = this.strings.formatStringFromName("responsiveUI.customNamePromptMsg", [w, h], 2);
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
    this.presets.sort(function RUI_sortPresets(aPresetA, aPresetB) {
      // We keep custom preset at first
      if (aPresetA.custom && !aPresetB.custom) {
        return 1;
      }
      if (!aPresetA.custom && aPresetB.custom) {
        return -1;
      }

      if (aPresetA.width === aPresetB.width) {
        if (aPresetA.height === aPresetB.height) {
          return 0;
        } else {
          return aPresetA.height > aPresetB.height;
        }
      } else {
        return aPresetA.width > aPresetB.width;
      }
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
  removePreset: function RUI_removePreset() {
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

    this.setSize(w, h);

    this.savePresets();
  },

  /**
   * Swap width and height.
   */
  rotate: function RUI_rotate() {
    let selectedPreset = this.menuitems.get(this.selectedItem);
    let width = this.rotateValue ? selectedPreset.height : selectedPreset.width;
    let height = this.rotateValue ? selectedPreset.width : selectedPreset.height;

    this.setSize(height, width);

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
   * @param aFileName name of the screenshot file (used for tests).
   */
  screenshot: function RUI_screenshot(aFileName) {
    let filename = aFileName;
    if (!filename) {
      let date = new Date();
      let month = ("0" + (date.getMonth() + 1)).substr(-2, 2);
      let day = ("0" + date.getDate()).substr(-2, 2);
      let dateString = [date.getFullYear(), month, day].join("-");
      let timeString = date.toTimeString().replace(/:/g, ".").split(" ")[0];
      filename = this.strings.formatStringFromName("responsiveUI.screenshotGeneratedFilename", [dateString, timeString], 2);
    }
    let mm = this.tab.linkedBrowser.messageManager;
    let chromeWindow = this.chromeDoc.defaultView;
    let doc = chromeWindow.document;
    function onScreenshot(aMessage) {
      mm.removeMessageListener("ResponsiveMode:RequestScreenshot:Done", onScreenshot);
      chromeWindow.saveURL(aMessage.data, filename + ".png", null, true, true, doc.documentURIObject, doc);
    }
    mm.addMessageListener("ResponsiveMode:RequestScreenshot:Done", onScreenshot);
    mm.sendAsyncMessage("ResponsiveMode:RequestScreenshot");
  },

  /**
   * Enable/Disable mouse -> touch events translation.
   */
   enableTouch: function RUI_enableTouch() {
     this.touchbutton.setAttribute("checked", "true");
     return this.touchEventSimulator.start();
   },

   disableTouch: function RUI_disableTouch() {
     this.touchbutton.removeAttribute("checked");
     return this.touchEventSimulator.stop();
   },

   hideTouchNotification: function RUI_hideTouchNotification() {
     let nbox = this.mainWindow.gBrowser.getNotificationBox(this.browser);
     let n = nbox.getNotificationWithValue("responsive-ui-need-reload");
     if (n) {
       n.close();
     }
   },

   toggleTouch: Task.async(function*() {
     this.hideTouchNotification();
     if (this.touchEventSimulator.enabled) {
       this.disableTouch();
     } else {
       let isReloadNeeded = yield this.enableTouch();
       if (isReloadNeeded) {
         if (Services.prefs.getBoolPref("devtools.responsiveUI.no-reload-notification")) {
           return;
         }

         let nbox = this.mainWindow.gBrowser.getNotificationBox(this.browser);

         var buttons = [{
           label: this.strings.GetStringFromName("responsiveUI.notificationReload"),
           callback: () => {
             this.browser.reload();
           },
           accessKey: this.strings.GetStringFromName("responsiveUI.notificationReload_accesskey"),
         }, {
           label: this.strings.GetStringFromName("responsiveUI.dontShowReloadNotification"),
           callback: function() {
             Services.prefs.setBoolPref("devtools.responsiveUI.no-reload-notification", true);
           },
           accessKey: this.strings.GetStringFromName("responsiveUI.dontShowReloadNotification_accesskey"),
         }];

         nbox.appendNotification(
           this.strings.GetStringFromName("responsiveUI.needReload"),
           "responsive-ui-need-reload",
           null,
           nbox.PRIORITY_INFO_LOW,
           buttons);
       }
     }
   }),

  /**
   * Change the size of the browser.
   *
   * @param aWidth width of the browser.
   * @param aHeight height of the browser.
   */
  setSize: function RUI_setSize(aWidth, aHeight) {
    aWidth = Math.min(Math.max(aWidth, MIN_WIDTH), MAX_WIDTH);
    aHeight = Math.min(Math.max(aHeight, MIN_HEIGHT), MAX_HEIGHT);

    // We resize the containing stack.
    let style = "max-width: %width;" +
                "min-width: %width;" +
                "max-height: %height;" +
                "min-height: %height;";

    style = style.replace(/%width/g, aWidth + "px");
    style = style.replace(/%height/g, aHeight + "px");

    this.stack.setAttribute("style", style);

    if (!this.ignoreY)
      this.resizeBarV.setAttribute("top", Math.round(aHeight / 2));
    if (!this.ignoreX)
      this.resizeBarH.setAttribute("left", Math.round(aWidth / 2));

    let selectedPreset = this.menuitems.get(this.selectedItem);

    // We update the custom menuitem if we are using it
    if (selectedPreset.custom) {
      selectedPreset.width = aWidth;
      selectedPreset.height = aHeight;

      this.setMenuLabel(this.selectedItem, selectedPreset);
    }
  },

  /**
   * Start the process of resizing the browser.
   *
   * @param aEvent
   */
  startResizing: function RUI_startResizing(aEvent) {
    let selectedPreset = this.menuitems.get(this.selectedItem);

    if (!selectedPreset.custom) {
      this.customPreset.width = this.rotateValue ? selectedPreset.height : selectedPreset.width;
      this.customPreset.height = this.rotateValue ? selectedPreset.width : selectedPreset.height;

      let menuitem = this.customMenuitem;
      this.setMenuLabel(menuitem, this.customPreset);

      this.currentPresetKey = this.customPreset.key;
      this.menulist.selectedItem = menuitem;
    }
    this.mainWindow.addEventListener("mouseup", this.bound_stopResizing, true);
    this.mainWindow.addEventListener("mousemove", this.bound_onDrag, true);
    this.container.style.pointerEvents = "none";

    this._resizing = true;
    this.stack.setAttribute("notransition", "true");

    this.lastScreenX = aEvent.screenX;
    this.lastScreenY = aEvent.screenY;

    this.ignoreY = (aEvent.target === this.resizeBarV);
    this.ignoreX = (aEvent.target === this.resizeBarH);

    this.isResizing = true;
  },

  /**
   * Resizing on mouse move.
   *
   * @param aEvent
   */
  onDrag: function RUI_onDrag(aEvent) {
    let shift = aEvent.shiftKey;
    let ctrl = !aEvent.shiftKey && aEvent.ctrlKey;

    let screenX = aEvent.screenX;
    let screenY = aEvent.screenY;

    let deltaX = screenX - this.lastScreenX;
    let deltaY = screenY - this.lastScreenY;

    if (this.ignoreY)
      deltaY = 0;
    if (this.ignoreX)
      deltaX = 0;

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

    this.setSize(width, height);
  },

  /**
   * Stop End resizing
   */
  stopResizing: function RUI_stopResizing() {
    this.container.style.pointerEvents = "auto";

    this.mainWindow.removeEventListener("mouseup", this.bound_stopResizing, true);
    this.mainWindow.removeEventListener("mousemove", this.bound_onDrag, true);

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
   saveCustomSize: function RUI_saveCustomSize() {
     Services.prefs.setIntPref("devtools.responsiveUI.customWidth", this.customPreset.width);
     Services.prefs.setIntPref("devtools.responsiveUI.customHeight", this.customPreset.height);
   },

  /**
   * Store the current preset as a pref.
   */
   saveCurrentPreset: function RUI_saveCurrentPreset() {
     Services.prefs.setCharPref("devtools.responsiveUI.currentPreset", this.currentPresetKey);
     Services.prefs.setBoolPref("devtools.responsiveUI.rotate", this.rotateValue);
   },

  /**
   * Store the list of all registered presets as a pref.
   */
  savePresets: function RUI_savePresets() {
    // We exclude the custom one
    let registeredPresets = this.presets.filter(function (aPreset) {
      return !aPreset.custom;
    });

    Services.prefs.setCharPref("devtools.responsiveUI.presets", JSON.stringify(registeredPresets));
  },
}

XPCOMUtils.defineLazyGetter(ResponsiveUI.prototype, "strings", function () {
  return Services.strings.createBundle("chrome://browser/locale/devtools/responsiveUI.properties");
});
