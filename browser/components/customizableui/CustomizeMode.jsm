/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["CustomizeMode", "_defaultImportantThemes"];

const kPrefCustomizationDebug = "browser.uiCustomization.debug";
const kPaletteId = "customization-palette";
const kDragDataTypePrefix = "text/toolbarwrapper-id/";
const kSkipSourceNodePref = "browser.uiCustomization.skipSourceNodeCheck";
const kDrawInTitlebarPref = "browser.tabs.drawInTitlebar";
const kCompactModeShowPref = "browser.compactmode.show";
const kBookmarksToolbarPref = "browser.toolbars.bookmarks.visibility";
const kKeepBroadcastAttributes = "keepbroadcastattributeswhencustomizing";

const kPanelItemContextMenu = "customizationPanelItemContextMenu";
const kPaletteItemContextMenu = "customizationPaletteItemContextMenu";

const kDownloadAutohideCheckboxId = "downloads-button-autohide-checkbox";
const kDownloadAutohidePanelId = "downloads-button-autohide-panel";
const kDownloadAutoHidePref = "browser.download.autohideButton";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { CustomizableUI } = ChromeUtils.import(
  "resource:///modules/CustomizableUI.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

XPCOMUtils.defineLazyGlobalGetters(this, ["CSS"]);

ChromeUtils.defineModuleGetter(
  this,
  "AddonManager",
  "resource://gre/modules/AddonManager.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "AMTelemetry",
  "resource://gre/modules/AddonManager.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "DragPositionManager",
  "resource:///modules/DragPositionManager.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "BrowserUsageTelemetry",
  "resource:///modules/BrowserUsageTelemetry.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "SessionStore",
  "resource:///modules/sessionstore/SessionStore.jsm"
);
XPCOMUtils.defineLazyGetter(this, "gWidgetsBundle", function() {
  const kUrl =
    "chrome://browser/locale/customizableui/customizableWidgets.properties";
  return Services.strings.createBundle(kUrl);
});
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "gProton",
  "browser.proton.enabled",
  false
);
XPCOMUtils.defineLazyServiceGetter(
  this,
  "gTouchBarUpdater",
  "@mozilla.org/widget/touchbarupdater;1",
  "nsITouchBarUpdater"
);

let gDebug;
XPCOMUtils.defineLazyGetter(this, "log", () => {
  let scope = {};
  ChromeUtils.import("resource://gre/modules/Console.jsm", scope);
  gDebug = Services.prefs.getBoolPref(kPrefCustomizationDebug, false);
  let consoleOptions = {
    maxLogLevel: gDebug ? "all" : "log",
    prefix: "CustomizeMode",
  };
  return new scope.ConsoleAPI(consoleOptions);
});

const DEFAULT_THEME_ID = "default-theme@mozilla.org";
const LIGHT_THEME_ID = "firefox-compact-light@mozilla.org";
const DARK_THEME_ID = "firefox-compact-dark@mozilla.org";
const ALPENGLOW_THEME_ID = "firefox-alpenglow@mozilla.org";

const _defaultImportantThemes = [
  DEFAULT_THEME_ID,
  LIGHT_THEME_ID,
  DARK_THEME_ID,
  ALPENGLOW_THEME_ID,
];

var gDraggingInToolbars;

var gTab;

function closeGlobalTab() {
  let win = gTab.ownerGlobal;
  if (win.gBrowser.browsers.length == 1) {
    win.BrowserOpenTab();
  }
  win.gBrowser.removeTab(gTab, { animate: true });
  gTab = null;
}

var gTabsProgressListener = {
  onLocationChange(aBrowser, aWebProgress, aRequest, aLocation, aFlags) {
    // Tear down customize mode when the customize mode tab loads some other page.
    // Customize mode will be re-entered if "about:blank" is loaded again, so
    // don't tear down in this case.
    if (
      !gTab ||
      gTab.linkedBrowser != aBrowser ||
      aLocation.spec == "about:blank"
    ) {
      return;
    }

    unregisterGlobalTab();
  },
};

function unregisterGlobalTab() {
  gTab.removeEventListener("TabClose", unregisterGlobalTab);
  let win = gTab.ownerGlobal;
  win.removeEventListener("unload", unregisterGlobalTab);
  win.gBrowser.removeTabsProgressListener(gTabsProgressListener);

  gTab.removeAttribute("customizemode");

  gTab = null;
}

function CustomizeMode(aWindow) {
  this.window = aWindow;
  this.document = aWindow.document;
  this.browser = aWindow.gBrowser;
  this.areas = new Set();

  this._translationObserver = new aWindow.MutationObserver(mutations =>
    this._onTranslations(mutations)
  );
  this._ensureCustomizationPanels();

  let content = this.$("customization-content-container");
  if (!content) {
    this.window.MozXULElement.insertFTLIfNeeded("browser/customizeMode.ftl");
    let container = this.$("customization-container");
    container.replaceChild(
      this.window.MozXULElement.parseXULToFragment(container.firstChild.data),
      container.lastChild
    );
  }
  // There are two palettes - there's the palette that can be overlayed with
  // toolbar items in browser.xhtml. This is invisible, and never seen by the
  // user. Then there's the visible palette, which gets populated and displayed
  // to the user when in customizing mode.
  this.visiblePalette = this.$(kPaletteId);
  this.pongArena = this.$("customization-pong-arena");

  if (this._canDrawInTitlebar()) {
    this._updateTitlebarCheckbox();
    Services.prefs.addObserver(kDrawInTitlebarPref, this);
  } else {
    this.$("customization-titlebar-visibility-checkbox").hidden = true;
  }

  // Observe pref changes to the bookmarks toolbar visibility,
  // since we won't get a toolbarvisibilitychange event if the
  // toolbar is changing from 'newtab' to 'always' in Customize mode
  // since the toolbar is shown with the 'newtab' setting.
  Services.prefs.addObserver(kBookmarksToolbarPref, this);

  this.window.addEventListener("unload", this);
}

CustomizeMode.prototype = {
  _changed: false,
  _transitioning: false,
  window: null,
  document: null,
  // areas is used to cache the customizable areas when in customization mode.
  areas: null,
  // When in customizing mode, we swap out the reference to the invisible
  // palette in gNavToolbox.palette for our visiblePalette. This way, for the
  // customizing browser window, when widgets are removed from customizable
  // areas and added to the palette, they're added to the visible palette.
  // _stowedPalette is a reference to the old invisible palette so we can
  // restore gNavToolbox.palette to its original state after exiting
  // customization mode.
  _stowedPalette: null,
  _dragOverItem: null,
  _customizing: false,
  _skipSourceNodeCheck: null,
  _mainViewContext: null,

  // These are the commands we continue to leave enabled while in customize mode.
  // All other commands are disabled, and we remove the disabled attribute when
  // leaving customize mode.
  _enabledCommands: new Set([
    "cmd_newNavigator",
    "cmd_newNavigatorTab",
    "cmd_newNavigatorTabNoEvent",
    "cmd_close",
    "cmd_closeWindow",
    "cmd_quitApplication",
    "View:FullScreen",
    "Browser:NextTab",
    "Browser:PrevTab",
    "Browser:NewUserContextTab",
    "Tools:PrivateBrowsing",
    "minimizeWindow",
    "zoomWindow",
  ]),

  get _handler() {
    return this.window.CustomizationHandler;
  },

  uninit() {
    if (this._canDrawInTitlebar()) {
      Services.prefs.removeObserver(kDrawInTitlebarPref, this);
    }
    Services.prefs.removeObserver(kBookmarksToolbarPref, this);
  },

  $(id) {
    return this.document.getElementById(id);
  },

  toggle() {
    if (
      this._handler.isEnteringCustomizeMode ||
      this._handler.isExitingCustomizeMode
    ) {
      this._wantToBeInCustomizeMode = !this._wantToBeInCustomizeMode;
      return;
    }
    if (this._customizing) {
      this.exit();
    } else {
      this.enter();
    }
  },

  async _updateThemeButtonIcon() {
    // Keep the default button icon.
    if (gProton) {
      return;
    }

    let lwthemeButton = this.$("customization-lwtheme-button");
    let lwthemeIcon = lwthemeButton.icon;
    let theme = (await AddonManager.getAddonsByTypes(["theme"])).find(
      addon => addon.isActive
    );
    lwthemeIcon.style.backgroundImage =
      theme && theme.iconURL ? "url(" + theme.iconURL + ")" : "";
  },

  setTab(aTab) {
    if (gTab == aTab) {
      return;
    }

    if (gTab) {
      closeGlobalTab();
    }

    gTab = aTab;

    gTab.setAttribute("customizemode", "true");
    SessionStore.persistTabAttribute("customizemode");

    if (gTab.linkedPanel) {
      gTab.linkedBrowser.stop();
    }

    let win = gTab.ownerGlobal;

    win.gBrowser.setTabTitle(gTab);
    win.gBrowser.setIcon(gTab, "chrome://browser/skin/customize.svg");

    gTab.addEventListener("TabClose", unregisterGlobalTab);

    win.gBrowser.addTabsProgressListener(gTabsProgressListener);

    win.addEventListener("unload", unregisterGlobalTab);

    if (gTab.selected) {
      win.gCustomizeMode.enter();
    }
  },

  enter() {
    if (!this.window.toolbar.visible) {
      let w = this.window.getTopWin(true);
      if (w) {
        w.gCustomizeMode.enter();
        return;
      }
      let obs = () => {
        Services.obs.removeObserver(obs, "browser-delayed-startup-finished");
        w = this.window.getTopWin(true);
        w.gCustomizeMode.enter();
      };
      Services.obs.addObserver(obs, "browser-delayed-startup-finished");
      this.window.openTrustedLinkIn("about:newtab", "window");
      return;
    }
    this._wantToBeInCustomizeMode = true;

    if (this._customizing || this._handler.isEnteringCustomizeMode) {
      return;
    }

    // Exiting; want to re-enter once we've done that.
    if (this._handler.isExitingCustomizeMode) {
      log.debug(
        "Attempted to enter while we're in the middle of exiting. " +
          "We'll exit after we've entered"
      );
      return;
    }

    if (!gTab) {
      this.setTab(
        this.browser.loadOneTab("about:blank", {
          inBackground: false,
          forceNotRemote: true,
          skipAnimation: true,
          triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
        })
      );
      return;
    }
    if (!gTab.selected) {
      // This will force another .enter() to be called via the
      // onlocationchange handler of the tabbrowser, so we return early.
      gTab.ownerGlobal.gBrowser.selectedTab = gTab;
      return;
    }
    gTab.ownerGlobal.focus();
    if (gTab.ownerDocument != this.document) {
      return;
    }

    let window = this.window;
    let document = this.document;

    this._handler.isEnteringCustomizeMode = true;

    // Always disable the reset button at the start of customize mode, it'll be re-enabled
    // if necessary when we finish entering:
    let resetButton = this.$("customization-reset-button");
    resetButton.setAttribute("disabled", "true");

    (async () => {
      // We shouldn't start customize mode until after browser-delayed-startup has finished:
      if (!this.window.gBrowserInit.delayedStartupFinished) {
        await new Promise(resolve => {
          let delayedStartupObserver = aSubject => {
            if (aSubject == this.window) {
              Services.obs.removeObserver(
                delayedStartupObserver,
                "browser-delayed-startup-finished"
              );
              resolve();
            }
          };

          Services.obs.addObserver(
            delayedStartupObserver,
            "browser-delayed-startup-finished"
          );
        });
      }

      CustomizableUI.dispatchToolboxEvent("beforecustomization", {}, window);
      CustomizableUI.notifyStartCustomizing(this.window);

      // Add a keypress listener to the document so that we can quickly exit
      // customization mode when pressing ESC.
      document.addEventListener("keypress", this);

      // Same goes for the menu button - if we're customizing, a click on the
      // menu button means a quick exit from customization mode.
      window.PanelUI.hide();

      let panelHolder = document.getElementById("customization-panelHolder");
      let panelContextMenu = document.getElementById(kPanelItemContextMenu);
      this._previousPanelContextMenuParent = panelContextMenu.parentNode;
      document.getElementById("mainPopupSet").appendChild(panelContextMenu);
      panelHolder.appendChild(window.PanelUI.overflowFixedList);

      window.PanelUI.overflowFixedList.setAttribute("customizing", true);
      window.PanelUI.menuButton.disabled = true;
      document.getElementById("nav-bar-overflow-button").disabled = true;

      this._transitioning = true;

      let customizer = document.getElementById("customization-container");
      let browser = document.getElementById("browser");
      browser.collapsed = true;
      customizer.hidden = false;

      this._wrapToolbarItemSync(CustomizableUI.AREA_TABSTRIP);

      this.document.documentElement.setAttribute("customizing", true);

      let customizableToolbars = document.querySelectorAll(
        "toolbar[customizable=true]:not([autohide=true], [collapsed=true])"
      );
      for (let toolbar of customizableToolbars) {
        toolbar.setAttribute("customizing", true);
      }

      this._updateOverflowPanelArrowOffset();

      // Let everybody in this window know that we're about to customize.
      CustomizableUI.dispatchToolboxEvent("customizationstarting", {}, window);

      await this._wrapToolbarItems();
      this.populatePalette();

      this._setupPaletteDragging();

      window.gNavToolbox.addEventListener("toolbarvisibilitychange", this);

      this._updateResetButton();
      this._updateUndoResetButton();
      this._updateTouchBarButton();
      this._updateDensityMenu();

      this._skipSourceNodeCheck =
        Services.prefs.getPrefType(kSkipSourceNodePref) ==
          Ci.nsIPrefBranch.PREF_BOOL &&
        Services.prefs.getBoolPref(kSkipSourceNodePref);

      CustomizableUI.addListener(this);
      this._customizing = true;
      this._transitioning = false;

      // Show the palette now that the transition has finished.
      this.visiblePalette.hidden = false;
      window.setTimeout(() => {
        // Force layout reflow to ensure the animation runs,
        // and make it async so it doesn't affect the timing.
        this.visiblePalette.clientTop;
        this.visiblePalette.setAttribute("showing", "true");
      }, 0);
      this._updateEmptyPaletteNotice();

      this._updateThemeButtonIcon();
      AddonManager.addAddonListener(this);

      this._setupDownloadAutoHideToggle();

      this._handler.isEnteringCustomizeMode = false;

      CustomizableUI.dispatchToolboxEvent("customizationready", {}, window);

      if (!this._wantToBeInCustomizeMode) {
        this.exit();
      }
    })().catch(e => {
      log.error("Error entering customize mode", e);
      this._handler.isEnteringCustomizeMode = false;
      // Exit customize mode to ensure proper clean-up when entering failed.
      this.exit();
    });
  },

  exit() {
    this._wantToBeInCustomizeMode = false;

    if (!this._customizing || this._handler.isExitingCustomizeMode) {
      return;
    }

    // Entering; want to exit once we've done that.
    if (this._handler.isEnteringCustomizeMode) {
      log.debug(
        "Attempted to exit while we're in the middle of entering. " +
          "We'll exit after we've entered"
      );
      return;
    }

    if (this.resetting) {
      log.debug(
        "Attempted to exit while we're resetting. " +
          "We'll exit after resetting has finished."
      );
      return;
    }

    this._handler.isExitingCustomizeMode = true;

    this._translationObserver.disconnect();

    this._teardownDownloadAutoHideToggle();

    AddonManager.removeAddonListener(this);
    CustomizableUI.removeListener(this);

    let window = this.window;
    let document = this.document;

    document.removeEventListener("keypress", this);

    this.togglePong(false);

    // Disable the reset and undo reset buttons while transitioning:
    let resetButton = this.$("customization-reset-button");
    let undoResetButton = this.$("customization-undo-reset-button");
    undoResetButton.hidden = resetButton.disabled = true;

    this._transitioning = true;

    this._depopulatePalette();

    // We need to set this._customizing to false and remove the `customizing`
    // attribute before removing the tab or else
    // XULBrowserWindow.onLocationChange might think that we're still in
    // customization mode and need to exit it for a second time.
    this._customizing = false;
    document.documentElement.removeAttribute("customizing");

    if (this.browser.selectedTab == gTab) {
      closeGlobalTab();
    }

    let customizer = document.getElementById("customization-container");
    let browser = document.getElementById("browser");
    customizer.hidden = true;
    browser.collapsed = false;

    window.gNavToolbox.removeEventListener("toolbarvisibilitychange", this);

    this._teardownPaletteDragging();

    (async () => {
      await this._unwrapToolbarItems();

      // And drop all area references.
      this.areas.clear();

      // Let everybody in this window know that we're starting to
      // exit customization mode.
      CustomizableUI.dispatchToolboxEvent("customizationending", {}, window);

      window.PanelUI.menuButton.disabled = false;
      let overflowContainer = document.getElementById(
        "widget-overflow-mainView"
      ).firstElementChild;
      overflowContainer.appendChild(window.PanelUI.overflowFixedList);
      document.getElementById("nav-bar-overflow-button").disabled = false;
      let panelContextMenu = document.getElementById(kPanelItemContextMenu);
      this._previousPanelContextMenuParent.appendChild(panelContextMenu);

      let customizableToolbars = document.querySelectorAll(
        "toolbar[customizable=true]:not([autohide=true])"
      );
      for (let toolbar of customizableToolbars) {
        toolbar.removeAttribute("customizing");
      }

      this._maybeMoveDownloadsButtonToNavBar();

      delete this._lastLightweightTheme;
      this._changed = false;
      this._transitioning = false;
      this._handler.isExitingCustomizeMode = false;
      CustomizableUI.dispatchToolboxEvent("aftercustomization", {}, window);
      CustomizableUI.notifyEndCustomizing(window);

      if (this._wantToBeInCustomizeMode) {
        this.enter();
      }
    })().catch(e => {
      log.error("Error exiting customize mode", e);
      this._handler.isExitingCustomizeMode = false;
    });
  },

  /**
   * The overflow panel in customize mode should have its arrow pointing
   * at the overflow button. In order to do this correctly, we pass the
   * distance between the inside of window and the middle of the button
   * to the customize mode markup in which the arrow and panel are placed.
   */
  async _updateOverflowPanelArrowOffset() {
    let currentDensity = this.document.documentElement.getAttribute(
      "uidensity"
    );
    let offset = await this.window.promiseDocumentFlushed(() => {
      let overflowButton = this.$("nav-bar-overflow-button");
      let buttonRect = overflowButton.getBoundingClientRect();
      let endDistance;
      if (this.window.RTL_UI) {
        endDistance = buttonRect.left;
      } else {
        endDistance = this.window.innerWidth - buttonRect.right;
      }
      return endDistance + buttonRect.width / 2;
    });
    if (
      !this.document ||
      currentDensity != this.document.documentElement.getAttribute("uidensity")
    ) {
      return;
    }
    this.$("customization-panelWrapper").style.setProperty(
      "--panel-arrow-offset",
      offset + "px"
    );
  },

  _getCustomizableChildForNode(aNode) {
    // NB: adjusted from _getCustomizableParent to keep that method fast
    // (it's used during drags), and avoid multiple DOM loops
    let areas = CustomizableUI.areas;
    // Caching this length is important because otherwise we'll also iterate
    // over items we add to the end from within the loop.
    let numberOfAreas = areas.length;
    for (let i = 0; i < numberOfAreas; i++) {
      let area = areas[i];
      let areaNode = aNode.ownerDocument.getElementById(area);
      let customizationTarget = CustomizableUI.getCustomizationTarget(areaNode);
      if (customizationTarget && customizationTarget != areaNode) {
        areas.push(customizationTarget.id);
      }
      let overflowTarget = areaNode && areaNode.getAttribute("overflowtarget");
      if (overflowTarget) {
        areas.push(overflowTarget);
      }
    }
    areas.push(kPaletteId);

    while (aNode && aNode.parentNode) {
      let parent = aNode.parentNode;
      if (areas.includes(parent.id)) {
        return aNode;
      }
      aNode = parent;
    }
    return null;
  },

  _promiseWidgetAnimationOut(aNode) {
    if (
      this.window.gReduceMotion ||
      aNode.getAttribute("cui-anchorid") == "nav-bar-overflow-button" ||
      (aNode.tagName != "toolbaritem" && aNode.tagName != "toolbarbutton") ||
      (aNode.id == "downloads-button" && aNode.hidden)
    ) {
      return null;
    }

    let animationNode;
    if (aNode.parentNode && aNode.parentNode.id.startsWith("wrapper-")) {
      animationNode = aNode.parentNode;
    } else {
      animationNode = aNode;
    }
    return new Promise(resolve => {
      function cleanupCustomizationExit() {
        resolveAnimationPromise();
      }

      function cleanupWidgetAnimationEnd(e) {
        if (
          e.animationName == "widget-animate-out" &&
          e.target.id == animationNode.id
        ) {
          resolveAnimationPromise();
        }
      }

      function resolveAnimationPromise() {
        animationNode.removeEventListener(
          "animationend",
          cleanupWidgetAnimationEnd
        );
        animationNode.removeEventListener(
          "customizationending",
          cleanupCustomizationExit
        );
        resolve(animationNode);
      }

      // Wait until the next frame before setting the class to ensure
      // we do start the animation.
      this.window.requestAnimationFrame(() => {
        this.window.requestAnimationFrame(() => {
          animationNode.classList.add("animate-out");
          animationNode.ownerGlobal.gNavToolbox.addEventListener(
            "customizationending",
            cleanupCustomizationExit
          );
          animationNode.addEventListener(
            "animationend",
            cleanupWidgetAnimationEnd
          );
        });
      });
    });
  },

  async addToToolbar(aNode, aReason) {
    aNode = this._getCustomizableChildForNode(aNode);
    if (aNode.localName == "toolbarpaletteitem" && aNode.firstElementChild) {
      aNode = aNode.firstElementChild;
    }
    let widgetAnimationPromise = this._promiseWidgetAnimationOut(aNode);
    let animationNode;
    if (widgetAnimationPromise) {
      animationNode = await widgetAnimationPromise;
    }

    let widgetToAdd = aNode.id;
    if (
      CustomizableUI.isSpecialWidget(widgetToAdd) &&
      aNode.closest("#customization-palette")
    ) {
      widgetToAdd = widgetToAdd.match(
        /^customizableui-special-(spring|spacer|separator)/
      )[1];
    }

    CustomizableUI.addWidgetToArea(widgetToAdd, CustomizableUI.AREA_NAVBAR);
    BrowserUsageTelemetry.recordWidgetChange(
      widgetToAdd,
      CustomizableUI.AREA_NAVBAR
    );
    if (!this._customizing) {
      CustomizableUI.dispatchToolboxEvent("customizationchange");
    }

    // If the user explicitly moves this item, turn off autohide.
    if (aNode.id == "downloads-button") {
      Services.prefs.setBoolPref(kDownloadAutoHidePref, false);
      if (this._customizing) {
        this._showDownloadsAutoHidePanel();
      }
    }

    if (animationNode) {
      animationNode.classList.remove("animate-out");
    }
  },

  async addToPanel(aNode, aReason) {
    aNode = this._getCustomizableChildForNode(aNode);
    if (aNode.localName == "toolbarpaletteitem" && aNode.firstElementChild) {
      aNode = aNode.firstElementChild;
    }
    let widgetAnimationPromise = this._promiseWidgetAnimationOut(aNode);
    let animationNode;
    if (widgetAnimationPromise) {
      animationNode = await widgetAnimationPromise;
    }

    let panel = CustomizableUI.AREA_FIXED_OVERFLOW_PANEL;
    CustomizableUI.addWidgetToArea(aNode.id, panel);
    BrowserUsageTelemetry.recordWidgetChange(aNode.id, panel, aReason);
    if (!this._customizing) {
      CustomizableUI.dispatchToolboxEvent("customizationchange");
    }

    // If the user explicitly moves this item, turn off autohide.
    if (aNode.id == "downloads-button") {
      Services.prefs.setBoolPref(kDownloadAutoHidePref, false);
      if (this._customizing) {
        this._showDownloadsAutoHidePanel();
      }
    }

    if (animationNode) {
      animationNode.classList.remove("animate-out");
    }
    if (!this.window.gReduceMotion) {
      let overflowButton = this.$("nav-bar-overflow-button");
      overflowButton.setAttribute("animate", "true");
      overflowButton.addEventListener("animationend", function onAnimationEnd(
        event
      ) {
        if (event.animationName.startsWith("overflow-animation")) {
          this.removeEventListener("animationend", onAnimationEnd);
          this.removeAttribute("animate");
        }
      });
    }
  },

  async removeFromArea(aNode, aReason) {
    aNode = this._getCustomizableChildForNode(aNode);
    if (aNode.localName == "toolbarpaletteitem" && aNode.firstElementChild) {
      aNode = aNode.firstElementChild;
    }
    let widgetAnimationPromise = this._promiseWidgetAnimationOut(aNode);
    let animationNode;
    if (widgetAnimationPromise) {
      animationNode = await widgetAnimationPromise;
    }

    CustomizableUI.removeWidgetFromArea(aNode.id);
    BrowserUsageTelemetry.recordWidgetChange(aNode.id, null, aReason);
    if (!this._customizing) {
      CustomizableUI.dispatchToolboxEvent("customizationchange");
    }

    // If the user explicitly removes this item, turn off autohide.
    if (aNode.id == "downloads-button") {
      Services.prefs.setBoolPref(kDownloadAutoHidePref, false);
      if (this._customizing) {
        this._showDownloadsAutoHidePanel();
      }
    }
    if (animationNode) {
      animationNode.classList.remove("animate-out");
    }
  },

  populatePalette() {
    let fragment = this.document.createDocumentFragment();
    let toolboxPalette = this.window.gNavToolbox.palette;

    try {
      let unusedWidgets = CustomizableUI.getUnusedWidgets(toolboxPalette);
      for (let widget of unusedWidgets) {
        let paletteItem = this.makePaletteItem(widget, "palette");
        if (!paletteItem) {
          continue;
        }
        fragment.appendChild(paletteItem);
      }

      let flexSpace = CustomizableUI.createSpecialWidget(
        "spring",
        this.document
      );
      fragment.appendChild(this.wrapToolbarItem(flexSpace, "palette"));

      this.visiblePalette.appendChild(fragment);
      this._stowedPalette = this.window.gNavToolbox.palette;
      this.window.gNavToolbox.palette = this.visiblePalette;

      // Now that the palette items are all here, disable all commands.
      // We do this here rather than directly in `enter` because we
      // need to do/undo this when we're called from reset(), too.
      this._updateCommandsDisabledState(true);
    } catch (ex) {
      log.error(ex);
    }
  },

  // XXXunf Maybe this should use -moz-element instead of wrapping the node?
  //       Would ensure no weird interactions/event handling from original node,
  //       and makes it possible to put this in a lazy-loaded iframe/real tab
  //       while still getting rid of the need for overlays.
  makePaletteItem(aWidget, aPlace) {
    let widgetNode = aWidget.forWindow(this.window).node;
    if (!widgetNode) {
      log.error(
        "Widget with id " + aWidget.id + " does not return a valid node"
      );
      return null;
    }
    // Do not build a palette item for hidden widgets; there's not much to show.
    if (widgetNode.hidden) {
      return null;
    }

    let wrapper = this.createOrUpdateWrapper(widgetNode, aPlace);
    wrapper.appendChild(widgetNode);
    return wrapper;
  },

  _depopulatePalette() {
    // Quick, undo the command disabling before we depopulate completely:
    this._updateCommandsDisabledState(false);

    this.visiblePalette.hidden = true;
    let paletteChild = this.visiblePalette.firstElementChild;
    let nextChild;
    while (paletteChild) {
      nextChild = paletteChild.nextElementSibling;
      let itemId = paletteChild.firstElementChild.id;
      if (CustomizableUI.isSpecialWidget(itemId)) {
        this.visiblePalette.removeChild(paletteChild);
      } else {
        // XXXunf Currently this doesn't destroy the (now unused) node in the
        //       API provider case. It would be good to do so, but we need to
        //       keep strong refs to it in CustomizableUI (can't iterate of
        //       WeakMaps), and there's the question of what behavior
        //       wrappers should have if consumers keep hold of them.
        let unwrappedPaletteItem = this.unwrapToolbarItem(paletteChild);
        this._stowedPalette.appendChild(unwrappedPaletteItem);
      }

      paletteChild = nextChild;
    }
    this.visiblePalette.hidden = false;
    this.window.gNavToolbox.palette = this._stowedPalette;
  },

  _updateCommandsDisabledState(shouldBeDisabled) {
    for (let command of this.document.querySelectorAll("command")) {
      if (!command.id || !this._enabledCommands.has(command.id)) {
        if (shouldBeDisabled) {
          if (command.getAttribute("disabled") != "true") {
            command.setAttribute("disabled", true);
          } else {
            command.setAttribute("wasdisabled", true);
          }
        } else if (command.getAttribute("wasdisabled") != "true") {
          command.removeAttribute("disabled");
        } else {
          command.removeAttribute("wasdisabled");
        }
      }
    }
  },

  isCustomizableItem(aNode) {
    return (
      aNode.localName == "toolbarbutton" ||
      aNode.localName == "toolbaritem" ||
      aNode.localName == "toolbarseparator" ||
      aNode.localName == "toolbarspring" ||
      aNode.localName == "toolbarspacer"
    );
  },

  isWrappedToolbarItem(aNode) {
    return aNode.localName == "toolbarpaletteitem";
  },

  deferredWrapToolbarItem(aNode, aPlace) {
    return new Promise(resolve => {
      dispatchFunction(() => {
        let wrapper = this.wrapToolbarItem(aNode, aPlace);
        resolve(wrapper);
      });
    });
  },

  wrapToolbarItem(aNode, aPlace) {
    if (!this.isCustomizableItem(aNode)) {
      return aNode;
    }
    let wrapper = this.createOrUpdateWrapper(aNode, aPlace);

    // It's possible that this toolbar node is "mid-flight" and doesn't have
    // a parent, in which case we skip replacing it. This can happen if a
    // toolbar item has been dragged into the palette. In that case, we tell
    // CustomizableUI to remove the widget from its area before putting the
    // widget in the palette - so the node will have no parent.
    if (aNode.parentNode) {
      aNode = aNode.parentNode.replaceChild(wrapper, aNode);
    }
    wrapper.appendChild(aNode);
    return wrapper;
  },

  /**
   * Helper to set the label, either directly or to set up the translation
   * observer so we can set the label once it's available.
   */
  _updateWrapperLabel(aNode, aIsUpdate, aWrapper = aNode.parentElement) {
    if (aNode.hasAttribute("label")) {
      aWrapper.setAttribute("title", aNode.getAttribute("label"));
      aWrapper.setAttribute("tooltiptext", aNode.getAttribute("label"));
    } else if (aNode.hasAttribute("title")) {
      aWrapper.setAttribute("title", aNode.getAttribute("title"));
      aWrapper.setAttribute("tooltiptext", aNode.getAttribute("title"));
    } else if (aNode.hasAttribute("data-l10n-id") && !aIsUpdate) {
      this._translationObserver.observe(aNode, {
        attributes: true,
        attributeFilter: ["label", "title"],
      });
    }
  },

  /**
   * Called when a node without a label or title is updated.
   */
  _onTranslations(aMutations) {
    for (let mut of aMutations) {
      let { target } = mut;
      if (
        target.parentElement?.localName == "toolbarpaletteitem" &&
        (target.hasAttribute("label") || mut.target.hasAttribute("title"))
      ) {
        this._updateWrapperLabel(target, true);
      }
    }
  },

  createOrUpdateWrapper(aNode, aPlace, aIsUpdate) {
    let wrapper;
    if (
      aIsUpdate &&
      aNode.parentNode &&
      aNode.parentNode.localName == "toolbarpaletteitem"
    ) {
      wrapper = aNode.parentNode;
      aPlace = wrapper.getAttribute("place");
    } else {
      wrapper = this.document.createXULElement("toolbarpaletteitem");
      // "place" is used to show the label when it's sitting in the palette.
      wrapper.setAttribute("place", aPlace);
    }

    // Ensure the wrapped item doesn't look like it's in any special state, and
    // can't be interactved with when in the customization palette.
    // Note that some buttons opt out of this with the
    // keepbroadcastattributeswhencustomizing attribute.
    if (
      aNode.hasAttribute("command") &&
      aNode.getAttribute(kKeepBroadcastAttributes) != "true"
    ) {
      wrapper.setAttribute("itemcommand", aNode.getAttribute("command"));
      aNode.removeAttribute("command");
    }

    if (
      aNode.hasAttribute("observes") &&
      aNode.getAttribute(kKeepBroadcastAttributes) != "true"
    ) {
      wrapper.setAttribute("itemobserves", aNode.getAttribute("observes"));
      aNode.removeAttribute("observes");
    }

    if (aNode.getAttribute("checked") == "true") {
      wrapper.setAttribute("itemchecked", "true");
      aNode.removeAttribute("checked");
    }

    if (aNode.hasAttribute("id")) {
      wrapper.setAttribute("id", "wrapper-" + aNode.getAttribute("id"));
    }

    this._updateWrapperLabel(aNode, aIsUpdate, wrapper);

    if (aNode.hasAttribute("flex")) {
      wrapper.setAttribute("flex", aNode.getAttribute("flex"));
    }

    let removable =
      aPlace == "palette" || CustomizableUI.isWidgetRemovable(aNode);
    wrapper.setAttribute("removable", removable);

    // Allow touch events to initiate dragging in customize mode.
    // This is only supported on Windows for now.
    wrapper.setAttribute("touchdownstartsdrag", "true");

    let contextMenuAttrName = "";
    if (aNode.getAttribute("context")) {
      contextMenuAttrName = "context";
    } else if (aNode.getAttribute("contextmenu")) {
      contextMenuAttrName = "contextmenu";
    }
    let currentContextMenu = aNode.getAttribute(contextMenuAttrName);
    let contextMenuForPlace =
      aPlace == "menu-panel" ? kPanelItemContextMenu : kPaletteItemContextMenu;
    if (aPlace != "toolbar") {
      wrapper.setAttribute("context", contextMenuForPlace);
    }
    // Only keep track of the menu if it is non-default.
    if (currentContextMenu && currentContextMenu != contextMenuForPlace) {
      aNode.setAttribute("wrapped-context", currentContextMenu);
      aNode.setAttribute("wrapped-contextAttrName", contextMenuAttrName);
      aNode.removeAttribute(contextMenuAttrName);
    } else if (currentContextMenu == contextMenuForPlace) {
      aNode.removeAttribute(contextMenuAttrName);
    }

    // Only add listeners for newly created wrappers:
    if (!aIsUpdate) {
      wrapper.addEventListener("mousedown", this);
      wrapper.addEventListener("mouseup", this);
    }

    if (CustomizableUI.isSpecialWidget(aNode.id)) {
      wrapper.setAttribute(
        "title",
        gWidgetsBundle.GetStringFromName(aNode.nodeName + ".label")
      );
    }

    return wrapper;
  },

  deferredUnwrapToolbarItem(aWrapper) {
    return new Promise(resolve => {
      dispatchFunction(() => {
        let item = null;
        try {
          item = this.unwrapToolbarItem(aWrapper);
        } catch (ex) {
          Cu.reportError(ex);
        }
        resolve(item);
      });
    });
  },

  unwrapToolbarItem(aWrapper) {
    if (aWrapper.nodeName != "toolbarpaletteitem") {
      return aWrapper;
    }
    aWrapper.removeEventListener("mousedown", this);
    aWrapper.removeEventListener("mouseup", this);

    let place = aWrapper.getAttribute("place");

    let toolbarItem = aWrapper.firstElementChild;
    if (!toolbarItem) {
      log.error(
        "no toolbarItem child for " + aWrapper.tagName + "#" + aWrapper.id
      );
      aWrapper.remove();
      return null;
    }

    if (aWrapper.hasAttribute("itemobserves")) {
      toolbarItem.setAttribute(
        "observes",
        aWrapper.getAttribute("itemobserves")
      );
    }

    if (aWrapper.hasAttribute("itemchecked")) {
      toolbarItem.checked = true;
    }

    if (aWrapper.hasAttribute("itemcommand")) {
      let commandID = aWrapper.getAttribute("itemcommand");
      toolbarItem.setAttribute("command", commandID);

      // XXX Bug 309953 - toolbarbuttons aren't in sync with their commands after customizing
      let command = this.$(commandID);
      if (command && command.hasAttribute("disabled")) {
        toolbarItem.setAttribute("disabled", command.getAttribute("disabled"));
      }
    }

    let wrappedContext = toolbarItem.getAttribute("wrapped-context");
    if (wrappedContext) {
      let contextAttrName = toolbarItem.getAttribute("wrapped-contextAttrName");
      toolbarItem.setAttribute(contextAttrName, wrappedContext);
      toolbarItem.removeAttribute("wrapped-contextAttrName");
      toolbarItem.removeAttribute("wrapped-context");
    } else if (place == "menu-panel") {
      toolbarItem.setAttribute("context", kPanelItemContextMenu);
    }

    if (aWrapper.parentNode) {
      aWrapper.parentNode.replaceChild(toolbarItem, aWrapper);
    }
    return toolbarItem;
  },

  async _wrapToolbarItem(aArea) {
    let target = CustomizableUI.getCustomizeTargetForArea(aArea, this.window);
    if (!target || this.areas.has(target)) {
      return null;
    }

    this._addDragHandlers(target);
    for (let child of target.children) {
      if (this.isCustomizableItem(child) && !this.isWrappedToolbarItem(child)) {
        await this.deferredWrapToolbarItem(
          child,
          CustomizableUI.getPlaceForItem(child)
        ).catch(log.error);
      }
    }
    this.areas.add(target);
    return target;
  },

  _wrapToolbarItemSync(aArea) {
    let target = CustomizableUI.getCustomizeTargetForArea(aArea, this.window);
    if (!target || this.areas.has(target)) {
      return null;
    }

    this._addDragHandlers(target);
    try {
      for (let child of target.children) {
        if (
          this.isCustomizableItem(child) &&
          !this.isWrappedToolbarItem(child)
        ) {
          this.wrapToolbarItem(child, CustomizableUI.getPlaceForItem(child));
        }
      }
    } catch (ex) {
      log.error(ex, ex.stack);
    }

    this.areas.add(target);
    return target;
  },

  async _wrapToolbarItems() {
    for (let area of CustomizableUI.areas) {
      await this._wrapToolbarItem(area);
    }
  },

  _addDragHandlers(aTarget) {
    // Allow dropping on the padding of the arrow panel.
    if (aTarget.id == CustomizableUI.AREA_FIXED_OVERFLOW_PANEL) {
      aTarget = this.$("customization-panelHolder");
    }
    aTarget.addEventListener("dragstart", this, true);
    aTarget.addEventListener("dragover", this, true);
    aTarget.addEventListener("dragexit", this, true);
    aTarget.addEventListener("drop", this, true);
    aTarget.addEventListener("dragend", this, true);
  },

  _wrapItemsInArea(target) {
    for (let child of target.children) {
      if (this.isCustomizableItem(child)) {
        this.wrapToolbarItem(child, CustomizableUI.getPlaceForItem(child));
      }
    }
  },

  _removeDragHandlers(aTarget) {
    // Remove handler from different target if it was added to
    // allow dropping on the padding of the arrow panel.
    if (aTarget.id == CustomizableUI.AREA_FIXED_OVERFLOW_PANEL) {
      aTarget = this.$("customization-panelHolder");
    }
    aTarget.removeEventListener("dragstart", this, true);
    aTarget.removeEventListener("dragover", this, true);
    aTarget.removeEventListener("dragexit", this, true);
    aTarget.removeEventListener("drop", this, true);
    aTarget.removeEventListener("dragend", this, true);
  },

  _unwrapItemsInArea(target) {
    for (let toolbarItem of target.children) {
      if (this.isWrappedToolbarItem(toolbarItem)) {
        this.unwrapToolbarItem(toolbarItem);
      }
    }
  },

  _unwrapToolbarItems() {
    return (async () => {
      for (let target of this.areas) {
        for (let toolbarItem of target.children) {
          if (this.isWrappedToolbarItem(toolbarItem)) {
            await this.deferredUnwrapToolbarItem(toolbarItem);
          }
        }
        this._removeDragHandlers(target);
      }
      this.areas.clear();
    })().catch(log.error);
  },

  reset() {
    this.resetting = true;
    // Disable the reset button temporarily while resetting:
    let btn = this.$("customization-reset-button");
    btn.disabled = true;
    return (async () => {
      this._depopulatePalette();
      await this._unwrapToolbarItems();

      CustomizableUI.reset();

      await this._wrapToolbarItems();
      this.populatePalette();

      this._updateResetButton();
      this._updateUndoResetButton();
      this._updateEmptyPaletteNotice();
      this._moveDownloadsButtonToNavBar = false;
      this.resetting = false;
      if (!this._wantToBeInCustomizeMode) {
        this.exit();
      }
    })().catch(log.error);
  },

  undoReset() {
    this.resetting = true;

    return (async () => {
      this._depopulatePalette();
      await this._unwrapToolbarItems();

      CustomizableUI.undoReset();

      await this._wrapToolbarItems();
      this.populatePalette();

      this._updateResetButton();
      this._updateUndoResetButton();
      this._updateEmptyPaletteNotice();
      this._moveDownloadsButtonToNavBar = false;
      this.resetting = false;
    })().catch(log.error);
  },

  _onToolbarVisibilityChange(aEvent) {
    let toolbar = aEvent.target;
    if (
      aEvent.detail.visible &&
      toolbar.getAttribute("customizable") == "true"
    ) {
      toolbar.setAttribute("customizing", "true");
    } else {
      toolbar.removeAttribute("customizing");
    }
    this._onUIChange();
  },

  onWidgetMoved(aWidgetId, aArea, aOldPosition, aNewPosition) {
    this._onUIChange();
  },

  onWidgetAdded(aWidgetId, aArea, aPosition) {
    this._onUIChange();
  },

  onWidgetRemoved(aWidgetId, aArea) {
    this._onUIChange();
  },

  onWidgetBeforeDOMChange(aNodeToChange, aSecondaryNode, aContainer) {
    if (aContainer.ownerGlobal != this.window || this.resetting) {
      return;
    }
    // If we get called for widgets that aren't in the window yet, they might not have
    // a parentNode at all.
    if (aNodeToChange.parentNode) {
      this.unwrapToolbarItem(aNodeToChange.parentNode);
    }
    if (aSecondaryNode) {
      this.unwrapToolbarItem(aSecondaryNode.parentNode);
    }
  },

  onWidgetAfterDOMChange(aNodeToChange, aSecondaryNode, aContainer) {
    if (aContainer.ownerGlobal != this.window || this.resetting) {
      return;
    }
    // If the node is still attached to the container, wrap it again:
    if (aNodeToChange.parentNode) {
      let place = CustomizableUI.getPlaceForItem(aNodeToChange);
      this.wrapToolbarItem(aNodeToChange, place);
      if (aSecondaryNode) {
        this.wrapToolbarItem(aSecondaryNode, place);
      }
    } else {
      // If not, it got removed.

      // If an API-based widget is removed while customizing, append it to the palette.
      // The _applyDrop code itself will take care of positioning it correctly, if
      // applicable. We need the code to be here so removing widgets using CustomizableUI's
      // API also does the right thing (and adds it to the palette)
      let widgetId = aNodeToChange.id;
      let widget = CustomizableUI.getWidget(widgetId);
      if (widget.provider == CustomizableUI.PROVIDER_API) {
        let paletteItem = this.makePaletteItem(widget, "palette");
        this.visiblePalette.appendChild(paletteItem);
      }
    }
  },

  onWidgetDestroyed(aWidgetId) {
    let wrapper = this.$("wrapper-" + aWidgetId);
    if (wrapper) {
      wrapper.remove();
    }
  },

  onWidgetAfterCreation(aWidgetId, aArea) {
    // If the node was added to an area, we would have gotten an onWidgetAdded notification,
    // plus associated DOM change notifications, so only do stuff for the palette:
    if (!aArea) {
      let widgetNode = this.$(aWidgetId);
      if (widgetNode) {
        this.wrapToolbarItem(widgetNode, "palette");
      } else {
        let widget = CustomizableUI.getWidget(aWidgetId);
        this.visiblePalette.appendChild(
          this.makePaletteItem(widget, "palette")
        );
      }
    }
  },

  onAreaNodeRegistered(aArea, aContainer) {
    if (aContainer.ownerDocument == this.document) {
      this._wrapItemsInArea(aContainer);
      this._addDragHandlers(aContainer);
      this.areas.add(aContainer);
    }
  },

  onAreaNodeUnregistered(aArea, aContainer, aReason) {
    if (
      aContainer.ownerDocument == this.document &&
      aReason == CustomizableUI.REASON_AREA_UNREGISTERED
    ) {
      this._unwrapItemsInArea(aContainer);
      this._removeDragHandlers(aContainer);
      this.areas.delete(aContainer);
    }
  },

  openAddonsManagerThemes(aEvent) {
    aEvent.target.parentNode.parentNode.hidePopup();
    AMTelemetry.recordLinkEvent({ object: "customize", value: "manageThemes" });
    this.window.BrowserOpenAddonsMgr("addons://list/theme");
  },

  getMoreThemes(aEvent) {
    aEvent.target.parentNode.parentNode.hidePopup();
    AMTelemetry.recordLinkEvent({ object: "customize", value: "getThemes" });
    let getMoreURL = Services.urlFormatter.formatURLPref(
      "lightweightThemes.getMoreURL"
    );
    this.window.openTrustedLinkIn(getMoreURL, "tab");
  },

  updateUIDensity(mode) {
    this.window.gUIDensity.update(mode);
    this._updateOverflowPanelArrowOffset();
  },

  setUIDensity(mode) {
    let win = this.window;
    let gUIDensity = win.gUIDensity;
    let currentDensity = gUIDensity.getCurrentDensity();
    let panel = win.document.getElementById("customization-uidensity-menu");

    Services.prefs.setIntPref(gUIDensity.uiDensityPref, mode);

    // If the user is choosing a different UI density mode while
    // the mode is overriden to Touch, remove the override.
    if (currentDensity.overridden) {
      Services.prefs.setBoolPref(gUIDensity.autoTouchModePref, false);
    }

    this._onUIChange();
    panel.hidePopup();
    this._updateOverflowPanelArrowOffset();
  },

  resetUIDensity() {
    this.window.gUIDensity.update();
    this._updateOverflowPanelArrowOffset();
  },

  onUIDensityMenuShowing() {
    let win = this.window;
    let doc = win.document;
    let gUIDensity = win.gUIDensity;
    let currentDensity = gUIDensity.getCurrentDensity();

    let normalItem = doc.getElementById(
      "customization-uidensity-menuitem-normal"
    );
    normalItem.mode = gUIDensity.MODE_NORMAL;

    let items = [normalItem];

    let compactItem = doc.getElementById(
      "customization-uidensity-menuitem-compact"
    );
    compactItem.mode = gUIDensity.MODE_COMPACT;

    if (Services.prefs.getBoolPref(kCompactModeShowPref)) {
      compactItem.hidden = false;
      items.push(compactItem);
    } else {
      compactItem.hidden = true;
    }

    let touchItem = doc.getElementById(
      "customization-uidensity-menuitem-touch"
    );
    // Touch mode can not be enabled in OSX right now.
    if (touchItem) {
      touchItem.mode = gUIDensity.MODE_TOUCH;
      items.push(touchItem);
    }

    // Mark the active mode menuitem.
    for (let item of items) {
      if (item.mode == currentDensity.mode) {
        item.setAttribute("aria-checked", "true");
        item.setAttribute("active", "true");
      } else {
        item.removeAttribute("aria-checked");
        item.removeAttribute("active");
      }
    }

    // Add menu items for automatically switching to Touch mode in Windows Tablet Mode,
    // which is only available in Windows 10.
    if (AppConstants.isPlatformAndVersionAtLeast("win", "10")) {
      let spacer = doc.getElementById("customization-uidensity-touch-spacer");
      let checkbox = doc.getElementById(
        "customization-uidensity-autotouchmode-checkbox"
      );
      spacer.removeAttribute("hidden");
      checkbox.removeAttribute("hidden");

      // Show a hint that the UI density was overridden automatically.
      if (currentDensity.overridden) {
        let sb = Services.strings.createBundle(
          "chrome://browser/locale/uiDensity.properties"
        );
        touchItem.setAttribute(
          "acceltext",
          sb.GetStringFromName("uiDensity.menuitem-touch.acceltext")
        );
      } else {
        touchItem.removeAttribute("acceltext");
      }

      let autoTouchMode = Services.prefs.getBoolPref(
        win.gUIDensity.autoTouchModePref
      );
      if (autoTouchMode) {
        checkbox.setAttribute("checked", "true");
      } else {
        checkbox.removeAttribute("checked");
      }
    }
  },

  updateAutoTouchMode(checked) {
    Services.prefs.setBoolPref("browser.touchmode.auto", checked);
    // Re-render the menu items since the active mode might have
    // change because of this.
    this.onUIDensityMenuShowing();
    this._onUIChange();
  },

  async onThemesMenuShowing(aEvent) {
    const MAX_THEME_COUNT = 6;

    this._clearThemesMenu(aEvent.target);

    let onThemeSelected = panel => {
      // This causes us to call _onUIChange when the LWT actually changes,
      // so the restore defaults / undo reset button is updated correctly.
      this._nextThemeChangeUserTriggered = true;
      panel.hidePopup();
    };

    let doc = this.document;

    function buildToolbarButton(aTheme) {
      let tbb = doc.createXULElement("toolbarbutton");
      tbb.theme = aTheme;
      tbb.setAttribute("label", aTheme.name);
      tbb.setAttribute(
        "image",
        aTheme.iconURL || "chrome://mozapps/skin/extensions/themeGeneric.svg"
      );
      if (aTheme.description) {
        tbb.setAttribute("tooltiptext", aTheme.description);
      }
      tbb.setAttribute("tabindex", "0");
      tbb.classList.add("customization-lwtheme-menu-theme");
      let isActive = aTheme.isActive;
      tbb.setAttribute("aria-checked", isActive);
      tbb.setAttribute("role", "menuitemradio");
      if (isActive) {
        tbb.setAttribute("active", "true");
      }

      return tbb;
    }

    let themes = await AddonManager.getAddonsByTypes(["theme"]);
    let currentTheme = themes.find(theme => theme.isActive);

    // Move the current theme (if any) and the default themes to the start:
    let importantThemes = new Set(_defaultImportantThemes);
    if (currentTheme) {
      importantThemes.add(currentTheme.id);
    }
    let importantList = [];
    for (let importantTheme of importantThemes) {
      importantList.push(
        ...themes.splice(
          themes.findIndex(theme => theme.id == importantTheme),
          1
        )
      );
    }

    // Sort the remainder alphabetically:
    themes.sort((a, b) => a.name.localeCompare(b.name));
    themes = importantList.concat(themes);

    if (themes.length > MAX_THEME_COUNT) {
      themes.length = MAX_THEME_COUNT;
    }

    let footer = doc.getElementById("customization-lwtheme-menu-footer");
    let panel = footer.parentNode;
    for (let theme of themes) {
      let button = buildToolbarButton(theme);
      button.addEventListener("command", async () => {
        onThemeSelected(panel);
        await button.theme.enable();
        AMTelemetry.recordActionEvent({
          object: "customize",
          action: "enable",
          extra: { type: "theme", addonId: theme.id },
        });
      });
      panel.insertBefore(button, footer);
    }
  },

  _clearThemesMenu(panel) {
    let footer = this.$("customization-lwtheme-menu-footer");
    let element = footer;
    while (
      element.previousElementSibling &&
      element.previousElementSibling.localName == "toolbarbutton"
    ) {
      element.previousElementSibling.remove();
    }

    // Workaround for bug 1059934
    panel.removeAttribute("height");
  },

  _onUIChange() {
    this._changed = true;
    if (!this.resetting) {
      this._updateResetButton();
      this._updateUndoResetButton();
      this._updateEmptyPaletteNotice();
    }
    CustomizableUI.dispatchToolboxEvent("customizationchange");
  },

  _updateEmptyPaletteNotice() {
    let paletteItems = this.visiblePalette.getElementsByTagName(
      "toolbarpaletteitem"
    );
    let whimsyButton = this.$("whimsy-button");

    if (
      paletteItems.length == 1 &&
      paletteItems[0].id.includes("wrapper-customizableui-special-spring")
    ) {
      whimsyButton.hidden = false;
    } else {
      this.togglePong(false);
      whimsyButton.hidden = true;
    }
  },

  _updateResetButton() {
    let btn = this.$("customization-reset-button");
    btn.disabled = CustomizableUI.inDefaultState;
  },

  _updateUndoResetButton() {
    let undoResetButton = this.$("customization-undo-reset-button");
    undoResetButton.hidden = !CustomizableUI.canUndoReset;
  },

  _updateTouchBarButton() {
    if (AppConstants.platform != "macosx") {
      return;
    }
    let touchBarButton = this.$("customization-touchbar-button");
    let touchBarSpacer = this.$("customization-touchbar-spacer");

    let isTouchBarInitialized = gTouchBarUpdater.isTouchBarInitialized();
    touchBarButton.hidden = !isTouchBarInitialized;
    touchBarSpacer.hidden = !isTouchBarInitialized;
  },

  _updateDensityMenu() {
    // If we're entering Customize Mode, and we're using compact mode,
    // then show the button after that.
    let gUIDensity = this.window.gUIDensity;
    if (gUIDensity.getCurrentDensity().mode == gUIDensity.MODE_COMPACT) {
      Services.prefs.setBoolPref(kCompactModeShowPref, true);
    }

    let button = this.document.getElementById("customization-uidensity-button");
    button.hidden =
      !Services.prefs.getBoolPref(kCompactModeShowPref) &&
      !button.querySelector("#customization-uidensity-menuitem-touch");
  },

  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "toolbarvisibilitychange":
        this._onToolbarVisibilityChange(aEvent);
        break;
      case "dragstart":
        this._onDragStart(aEvent);
        break;
      case "dragover":
        this._onDragOver(aEvent);
        break;
      case "drop":
        this._onDragDrop(aEvent);
        break;
      case "dragexit":
        this._onDragExit(aEvent);
        break;
      case "dragend":
        this._onDragEnd(aEvent);
        break;
      case "mousedown":
        this._onMouseDown(aEvent);
        break;
      case "mouseup":
        this._onMouseUp(aEvent);
        break;
      case "keypress":
        if (aEvent.keyCode == aEvent.DOM_VK_ESCAPE) {
          this.exit();
        }
        break;
      case "unload":
        this.uninit();
        break;
    }
  },

  /**
   * We handle dragover/drop on the outer palette separately
   * to avoid overlap with other drag/drop handlers.
   */
  _setupPaletteDragging() {
    this._addDragHandlers(this.visiblePalette);

    this.paletteDragHandler = aEvent => {
      let originalTarget = aEvent.originalTarget;
      if (
        this._isUnwantedDragDrop(aEvent) ||
        this.visiblePalette.contains(originalTarget) ||
        this.$("customization-panelHolder").contains(originalTarget)
      ) {
        return;
      }
      // We have a dragover/drop on the palette.
      if (aEvent.type == "dragover") {
        this._onDragOver(aEvent, this.visiblePalette);
      } else {
        this._onDragDrop(aEvent, this.visiblePalette);
      }
    };
    let contentContainer = this.$("customization-content-container");
    contentContainer.addEventListener(
      "dragover",
      this.paletteDragHandler,
      true
    );
    contentContainer.addEventListener("drop", this.paletteDragHandler, true);
  },

  _teardownPaletteDragging() {
    DragPositionManager.stop();
    this._removeDragHandlers(this.visiblePalette);

    let contentContainer = this.$("customization-content-container");
    contentContainer.removeEventListener(
      "dragover",
      this.paletteDragHandler,
      true
    );
    contentContainer.removeEventListener("drop", this.paletteDragHandler, true);
    delete this.paletteDragHandler;
  },

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "nsPref:changed":
        this._updateResetButton();
        this._updateUndoResetButton();
        if (this._canDrawInTitlebar()) {
          this._updateTitlebarCheckbox();
        }
        break;
    }
  },

  async onInstalled(addon) {
    await this.onEnabled(addon);
  },

  async onEnabled(addon) {
    if (addon.type != "theme") {
      return;
    }

    await this._updateThemeButtonIcon();
    if (this._nextThemeChangeUserTriggered) {
      this._onUIChange();
    }
    this._nextThemeChangeUserTriggered = false;
  },

  _canDrawInTitlebar() {
    return this.window.TabsInTitlebar.systemSupported;
  },

  _ensureCustomizationPanels() {
    let template = this.$("customizationPanel");
    template.replaceWith(template.content);

    let wrapper = this.$("customModeWrapper");
    wrapper.replaceWith(wrapper.content);
  },

  _updateTitlebarCheckbox() {
    let drawInTitlebar = Services.prefs.getBoolPref(
      kDrawInTitlebarPref,
      this.window.matchMedia("(-moz-gtk-csd-hide-titlebar-by-default)").matches
    );
    let checkbox = this.$("customization-titlebar-visibility-checkbox");
    // Drawing in the titlebar means 'hiding' the titlebar.
    // We use the attribute rather than a property because if we're not in
    // customize mode the button is hidden and properties don't work.
    if (drawInTitlebar) {
      checkbox.removeAttribute("checked");
    } else {
      checkbox.setAttribute("checked", "true");
    }
  },

  toggleTitlebar(aShouldShowTitlebar) {
    // Drawing in the titlebar means not showing the titlebar, hence the negation:
    Services.prefs.setBoolPref(kDrawInTitlebarPref, !aShouldShowTitlebar);
  },

  _getBoundsWithoutFlushing(element) {
    return this.window.windowUtils.getBoundsWithoutFlushing(element);
  },

  _onDragStart(aEvent) {
    __dumpDragData(aEvent);
    let item = aEvent.target;
    while (item && item.localName != "toolbarpaletteitem") {
      if (
        item.localName == "toolbar" ||
        item.id == kPaletteId ||
        item.id == "customization-panelHolder"
      ) {
        return;
      }
      item = item.parentNode;
    }

    let draggedItem = item.firstElementChild;
    let placeForItem = CustomizableUI.getPlaceForItem(item);

    let dt = aEvent.dataTransfer;
    let documentId = aEvent.target.ownerDocument.documentElement.id;

    dt.mozSetDataAt(kDragDataTypePrefix + documentId, draggedItem.id, 0);
    dt.effectAllowed = "move";

    let itemRect = this._getBoundsWithoutFlushing(draggedItem);
    let itemCenter = {
      x: itemRect.left + itemRect.width / 2,
      y: itemRect.top + itemRect.height / 2,
    };
    this._dragOffset = {
      x: aEvent.clientX - itemCenter.x,
      y: aEvent.clientY - itemCenter.y,
    };

    let toolbarParent = draggedItem.closest("toolbar");
    if (toolbarParent) {
      let toolbarRect = this._getBoundsWithoutFlushing(toolbarParent);
      toolbarParent.style.minHeight = toolbarRect.height + "px";
    }

    gDraggingInToolbars = new Set();

    // Hack needed so that the dragimage will still show the
    // item as it appeared before it was hidden.
    this._initializeDragAfterMove = () => {
      // For automated tests, we sometimes start exiting customization mode
      // before this fires, which leaves us with placeholders inserted after
      // we've exited. So we need to check that we are indeed customizing.
      if (this._customizing && !this._transitioning) {
        item.hidden = true;
        DragPositionManager.start(this.window);
        let canUsePrevSibling =
          placeForItem == "toolbar" || placeForItem == "menu-panel";
        if (item.nextElementSibling) {
          this._setDragActive(
            item.nextElementSibling,
            "before",
            draggedItem.id,
            placeForItem
          );
          this._dragOverItem = item.nextElementSibling;
        } else if (canUsePrevSibling && item.previousElementSibling) {
          this._setDragActive(
            item.previousElementSibling,
            "after",
            draggedItem.id,
            placeForItem
          );
          this._dragOverItem = item.previousElementSibling;
        }
        let currentArea = this._getCustomizableParent(item);
        currentArea.setAttribute("draggingover", "true");
      }
      this._initializeDragAfterMove = null;
      this.window.clearTimeout(this._dragInitializeTimeout);
    };
    this._dragInitializeTimeout = this.window.setTimeout(
      this._initializeDragAfterMove,
      0
    );
  },

  _onDragOver(aEvent, aOverrideTarget) {
    if (this._isUnwantedDragDrop(aEvent)) {
      return;
    }
    if (this._initializeDragAfterMove) {
      this._initializeDragAfterMove();
    }

    __dumpDragData(aEvent);

    let document = aEvent.target.ownerDocument;
    let documentId = document.documentElement.id;
    if (!aEvent.dataTransfer.mozTypesAt(0).length) {
      return;
    }

    let draggedItemId = aEvent.dataTransfer.mozGetDataAt(
      kDragDataTypePrefix + documentId,
      0
    );
    let draggedWrapper = document.getElementById("wrapper-" + draggedItemId);
    let targetArea = this._getCustomizableParent(
      aOverrideTarget || aEvent.currentTarget
    );
    let originArea = this._getCustomizableParent(draggedWrapper);

    // Do nothing if the target or origin are not customizable.
    if (!targetArea || !originArea) {
      return;
    }

    // Do nothing if the widget is not allowed to be removed.
    if (
      targetArea.id == kPaletteId &&
      !CustomizableUI.isWidgetRemovable(draggedItemId)
    ) {
      return;
    }

    // Do nothing if the widget is not allowed to move to the target area.
    if (
      targetArea.id != kPaletteId &&
      !CustomizableUI.canWidgetMoveToArea(draggedItemId, targetArea.id)
    ) {
      return;
    }

    let targetAreaType = CustomizableUI.getPlaceForItem(targetArea);
    let targetNode = this._getDragOverNode(
      aEvent,
      targetArea,
      targetAreaType,
      draggedItemId
    );

    // We need to determine the place that the widget is being dropped in
    // the target.
    let dragOverItem, dragValue;
    if (targetNode == CustomizableUI.getCustomizationTarget(targetArea)) {
      // We'll assume if the user is dragging directly over the target, that
      // they're attempting to append a child to that target.
      dragOverItem =
        (targetAreaType == "toolbar"
          ? this._findVisiblePreviousSiblingNode(targetNode.lastElementChild)
          : targetNode.lastElementChild) || targetNode;
      dragValue = "after";
    } else {
      let targetParent = targetNode.parentNode;
      let position = Array.prototype.indexOf.call(
        targetParent.children,
        targetNode
      );
      if (position == -1) {
        dragOverItem =
          targetAreaType == "toolbar"
            ? this._findVisiblePreviousSiblingNode(targetNode.lastElementChild)
            : targetNode.lastElementChild;
        dragValue = "after";
      } else {
        dragOverItem = targetParent.children[position];
        if (targetAreaType == "toolbar") {
          // Check if the aDraggedItem is hovered past the first half of dragOverItem
          let itemRect = this._getBoundsWithoutFlushing(dragOverItem);
          let dropTargetCenter = itemRect.left + itemRect.width / 2;
          let existingDir = dragOverItem.getAttribute("dragover");
          let dirFactor = this.window.RTL_UI ? -1 : 1;
          if (existingDir == "before") {
            dropTargetCenter +=
              ((parseInt(dragOverItem.style.borderInlineStartWidth) || 0) / 2) *
              dirFactor;
          } else {
            dropTargetCenter -=
              ((parseInt(dragOverItem.style.borderInlineEndWidth) || 0) / 2) *
              dirFactor;
          }
          let before = this.window.RTL_UI
            ? aEvent.clientX > dropTargetCenter
            : aEvent.clientX < dropTargetCenter;
          dragValue = before ? "before" : "after";
        } else if (targetAreaType == "menu-panel") {
          let itemRect = this._getBoundsWithoutFlushing(dragOverItem);
          let dropTargetCenter = itemRect.top + itemRect.height / 2;
          let existingDir = dragOverItem.getAttribute("dragover");
          if (existingDir == "before") {
            dropTargetCenter +=
              (parseInt(dragOverItem.style.borderBlockStartWidth) || 0) / 2;
          } else {
            dropTargetCenter -=
              (parseInt(dragOverItem.style.borderBlockEndWidth) || 0) / 2;
          }
          dragValue = aEvent.clientY < dropTargetCenter ? "before" : "after";
        } else {
          dragValue = "before";
        }
      }
    }

    if (this._dragOverItem && dragOverItem != this._dragOverItem) {
      this._cancelDragActive(this._dragOverItem, dragOverItem);
    }

    if (
      dragOverItem != this._dragOverItem ||
      dragValue != dragOverItem.getAttribute("dragover")
    ) {
      if (dragOverItem != CustomizableUI.getCustomizationTarget(targetArea)) {
        this._setDragActive(
          dragOverItem,
          dragValue,
          draggedItemId,
          targetAreaType
        );
      }
      this._dragOverItem = dragOverItem;
      targetArea.setAttribute("draggingover", "true");
    }

    aEvent.preventDefault();
    aEvent.stopPropagation();
  },

  _onDragDrop(aEvent, aOverrideTarget) {
    if (this._isUnwantedDragDrop(aEvent)) {
      return;
    }

    __dumpDragData(aEvent);
    this._initializeDragAfterMove = null;
    this.window.clearTimeout(this._dragInitializeTimeout);

    let targetArea = this._getCustomizableParent(
      aOverrideTarget || aEvent.currentTarget
    );
    let document = aEvent.target.ownerDocument;
    let documentId = document.documentElement.id;
    let draggedItemId = aEvent.dataTransfer.mozGetDataAt(
      kDragDataTypePrefix + documentId,
      0
    );
    let draggedWrapper = document.getElementById("wrapper-" + draggedItemId);
    let originArea = this._getCustomizableParent(draggedWrapper);
    if (this._dragSizeMap) {
      this._dragSizeMap = new WeakMap();
    }
    // Do nothing if the target area or origin area are not customizable.
    if (!targetArea || !originArea) {
      return;
    }
    let targetNode = this._dragOverItem;
    let dropDir = targetNode.getAttribute("dragover");
    // Need to insert *after* this node if we promised the user that:
    if (targetNode != targetArea && dropDir == "after") {
      if (targetNode.nextElementSibling) {
        targetNode = targetNode.nextElementSibling;
      } else {
        targetNode = targetArea;
      }
    }
    if (targetNode.tagName == "toolbarpaletteitem") {
      targetNode = targetNode.firstElementChild;
    }

    this._cancelDragActive(this._dragOverItem, null, true);

    try {
      this._applyDrop(
        aEvent,
        targetArea,
        originArea,
        draggedItemId,
        targetNode
      );
    } catch (ex) {
      log.error(ex, ex.stack);
    }

    // If the user explicitly moves this item, turn off autohide.
    if (draggedItemId == "downloads-button") {
      Services.prefs.setBoolPref(kDownloadAutoHidePref, false);
      this._showDownloadsAutoHidePanel();
    }
  },

  _applyDrop(aEvent, aTargetArea, aOriginArea, aDraggedItemId, aTargetNode) {
    let document = aEvent.target.ownerDocument;
    let draggedItem = document.getElementById(aDraggedItemId);
    draggedItem.hidden = false;
    draggedItem.removeAttribute("mousedown");

    let toolbarParent = draggedItem.closest("toolbar");
    if (toolbarParent) {
      toolbarParent.style.removeProperty("min-height");
    }

    // Do nothing if the target was dropped onto itself (ie, no change in area
    // or position).
    if (draggedItem == aTargetNode) {
      return;
    }

    // Is the target area the customization palette?
    if (aTargetArea.id == kPaletteId) {
      // Did we drag from outside the palette?
      if (aOriginArea.id !== kPaletteId) {
        if (!CustomizableUI.isWidgetRemovable(aDraggedItemId)) {
          return;
        }

        CustomizableUI.removeWidgetFromArea(aDraggedItemId, "drag");
        BrowserUsageTelemetry.recordWidgetChange(aDraggedItemId, null, "drag");
        // Special widgets are removed outright, we can return here:
        if (CustomizableUI.isSpecialWidget(aDraggedItemId)) {
          return;
        }
      }
      draggedItem = draggedItem.parentNode;

      // If the target node is the palette itself, just append
      if (aTargetNode == this.visiblePalette) {
        this.visiblePalette.appendChild(draggedItem);
      } else {
        // The items in the palette are wrapped, so we need the target node's parent here:
        this.visiblePalette.insertBefore(draggedItem, aTargetNode.parentNode);
      }
      this._onDragEnd(aEvent);
      return;
    }

    if (!CustomizableUI.canWidgetMoveToArea(aDraggedItemId, aTargetArea.id)) {
      return;
    }

    // Skipintoolbarset items won't really be moved:
    let areaCustomizationTarget = CustomizableUI.getCustomizationTarget(
      aTargetArea
    );
    if (draggedItem.getAttribute("skipintoolbarset") == "true") {
      // These items should never leave their area:
      if (aTargetArea != aOriginArea) {
        return;
      }
      let place = draggedItem.parentNode.getAttribute("place");
      this.unwrapToolbarItem(draggedItem.parentNode);
      if (aTargetNode == areaCustomizationTarget) {
        areaCustomizationTarget.appendChild(draggedItem);
      } else {
        this.unwrapToolbarItem(aTargetNode.parentNode);
        areaCustomizationTarget.insertBefore(draggedItem, aTargetNode);
        this.wrapToolbarItem(aTargetNode, place);
      }
      this.wrapToolbarItem(draggedItem, place);
      return;
    }

    // Force creating a new spacer/spring/separator if dragging from the palette
    if (
      CustomizableUI.isSpecialWidget(aDraggedItemId) &&
      aOriginArea.id == kPaletteId
    ) {
      aDraggedItemId = aDraggedItemId.match(
        /^customizableui-special-(spring|spacer|separator)/
      )[1];
    }

    // Is the target the customization area itself? If so, we just add the
    // widget to the end of the area.
    if (aTargetNode == areaCustomizationTarget) {
      CustomizableUI.addWidgetToArea(aDraggedItemId, aTargetArea.id);
      BrowserUsageTelemetry.recordWidgetChange(
        aDraggedItemId,
        aTargetArea.id,
        "drag"
      );
      this._onDragEnd(aEvent);
      return;
    }

    // We need to determine the place that the widget is being dropped in
    // the target.
    let placement;
    let itemForPlacement = aTargetNode;
    // Skip the skipintoolbarset items when determining the place of the item:
    while (
      itemForPlacement &&
      itemForPlacement.getAttribute("skipintoolbarset") == "true" &&
      itemForPlacement.parentNode &&
      itemForPlacement.parentNode.nodeName == "toolbarpaletteitem"
    ) {
      itemForPlacement = itemForPlacement.parentNode.nextElementSibling;
      if (
        itemForPlacement &&
        itemForPlacement.nodeName == "toolbarpaletteitem"
      ) {
        itemForPlacement = itemForPlacement.firstElementChild;
      }
    }
    if (itemForPlacement) {
      let targetNodeId =
        itemForPlacement.nodeName == "toolbarpaletteitem"
          ? itemForPlacement.firstElementChild &&
            itemForPlacement.firstElementChild.id
          : itemForPlacement.id;
      placement = CustomizableUI.getPlacementOfWidget(targetNodeId);
    }
    if (!placement) {
      log.debug(
        "Could not get a position for " +
          aTargetNode.nodeName +
          "#" +
          aTargetNode.id +
          "." +
          aTargetNode.className
      );
    }
    let position = placement ? placement.position : null;

    // Is the target area the same as the origin? Since we've already handled
    // the possibility that the target is the customization palette, we know
    // that the widget is moving within a customizable area.
    if (aTargetArea == aOriginArea) {
      CustomizableUI.moveWidgetWithinArea(aDraggedItemId, position);
      BrowserUsageTelemetry.recordWidgetChange(
        aDraggedItemId,
        aTargetArea.id,
        "drag"
      );
    } else {
      CustomizableUI.addWidgetToArea(aDraggedItemId, aTargetArea.id, position);
      BrowserUsageTelemetry.recordWidgetChange(
        aDraggedItemId,
        aTargetArea.id,
        "drag"
      );
    }

    this._onDragEnd(aEvent);

    // If we dropped onto a skipintoolbarset item, manually correct the drop location:
    if (aTargetNode != itemForPlacement) {
      let draggedWrapper = draggedItem.parentNode;
      let container = draggedWrapper.parentNode;
      container.insertBefore(draggedWrapper, aTargetNode.parentNode);
    }
  },

  _onDragExit(aEvent) {
    if (this._isUnwantedDragDrop(aEvent)) {
      return;
    }

    __dumpDragData(aEvent);

    // When leaving customization areas, cancel the drag on the last dragover item
    // We've attached the listener to areas, so aEvent.currentTarget will be the area.
    // We don't care about dragexit events fired on descendants of the area,
    // so we check that the event's target is the same as the area to which the listener
    // was attached.
    if (this._dragOverItem && aEvent.target == aEvent.currentTarget) {
      this._cancelDragActive(this._dragOverItem);
      this._dragOverItem = null;
    }
  },

  /**
   * To workaround bug 460801 we manually forward the drop event here when dragend wouldn't be fired.
   *
   * Note that that means that this function may be called multiple times by a single drag operation.
   */
  _onDragEnd(aEvent) {
    if (this._isUnwantedDragDrop(aEvent)) {
      return;
    }
    this._initializeDragAfterMove = null;
    this.window.clearTimeout(this._dragInitializeTimeout);
    __dumpDragData(aEvent, "_onDragEnd");

    let document = aEvent.target.ownerDocument;
    document.documentElement.removeAttribute("customizing-movingItem");

    let documentId = document.documentElement.id;
    if (!aEvent.dataTransfer.mozTypesAt(0)) {
      return;
    }

    let draggedItemId = aEvent.dataTransfer.mozGetDataAt(
      kDragDataTypePrefix + documentId,
      0
    );

    let draggedWrapper = document.getElementById("wrapper-" + draggedItemId);

    // DraggedWrapper might no longer available if a widget node is
    // destroyed after starting (but before stopping) a drag.
    if (draggedWrapper) {
      draggedWrapper.hidden = false;
      draggedWrapper.removeAttribute("mousedown");

      let toolbarParent = draggedWrapper.closest("toolbar");
      if (toolbarParent) {
        toolbarParent.style.removeProperty("min-height");
      }
    }

    if (this._dragOverItem) {
      this._cancelDragActive(this._dragOverItem);
      this._dragOverItem = null;
    }
    DragPositionManager.stop();
  },

  _isUnwantedDragDrop(aEvent) {
    // The synthesized events for tests generated by synthesizePlainDragAndDrop
    // and synthesizeDrop in mochitests are used only for testing whether the
    // right data is being put into the dataTransfer. Neither cause a real drop
    // to occur, so they don't set the source node. There isn't a means of
    // testing real drag and drops, so this pref skips the check but it should
    // only be set by test code.
    if (this._skipSourceNodeCheck) {
      return false;
    }

    /* Discard drag events that originated from a separate window to
       prevent content->chrome privilege escalations. */
    let mozSourceNode = aEvent.dataTransfer.mozSourceNode;
    // mozSourceNode is null in the dragStart event handler or if
    // the drag event originated in an external application.
    return !mozSourceNode || mozSourceNode.ownerGlobal != this.window;
  },

  _setDragActive(aItem, aValue, aDraggedItemId, aAreaType) {
    if (!aItem) {
      return;
    }

    if (aItem.getAttribute("dragover") != aValue) {
      aItem.setAttribute("dragover", aValue);

      let window = aItem.ownerGlobal;
      let draggedItem = window.document.getElementById(aDraggedItemId);
      if (aAreaType == "palette") {
        this._setGridDragActive(aItem, draggedItem, aValue);
      } else {
        let targetArea = this._getCustomizableParent(aItem);
        let makeSpaceImmediately = false;
        if (!gDraggingInToolbars.has(targetArea.id)) {
          gDraggingInToolbars.add(targetArea.id);
          let draggedWrapper = this.$("wrapper-" + aDraggedItemId);
          let originArea = this._getCustomizableParent(draggedWrapper);
          makeSpaceImmediately = originArea == targetArea;
        }
        let propertyToMeasure = aAreaType == "toolbar" ? "width" : "height";
        // Calculate width/height of the item when it'd be dropped in this position.
        let borderWidth = this._getDragItemSize(aItem, draggedItem)[
          propertyToMeasure
        ];
        let layoutSide = aAreaType == "toolbar" ? "Inline" : "Block";
        let prop, otherProp;
        if (aValue == "before") {
          prop = "border" + layoutSide + "StartWidth";
          otherProp = "border-" + layoutSide.toLowerCase() + "-end-width";
        } else {
          prop = "border" + layoutSide + "EndWidth";
          otherProp = "border-" + layoutSide.toLowerCase() + "-start-width";
        }
        if (makeSpaceImmediately) {
          aItem.setAttribute("notransition", "true");
        }
        aItem.style[prop] = borderWidth + "px";
        aItem.style.removeProperty(otherProp);
        if (makeSpaceImmediately) {
          // Force a layout flush:
          aItem.getBoundingClientRect();
          aItem.removeAttribute("notransition");
        }
      }
    }
  },
  _cancelDragActive(aItem, aNextItem, aNoTransition) {
    let currentArea = this._getCustomizableParent(aItem);
    if (!currentArea) {
      return;
    }
    let nextArea = aNextItem ? this._getCustomizableParent(aNextItem) : null;
    if (currentArea != nextArea) {
      currentArea.removeAttribute("draggingover");
    }
    let areaType = CustomizableUI.getAreaType(currentArea.id);
    if (areaType) {
      if (aNoTransition) {
        aItem.setAttribute("notransition", "true");
      }
      aItem.removeAttribute("dragover");
      // Remove all property values in the case that the end padding
      // had been set.
      aItem.style.removeProperty("border-inline-start-width");
      aItem.style.removeProperty("border-inline-end-width");
      aItem.style.removeProperty("border-block-start-width");
      aItem.style.removeProperty("border-block-end-width");
      if (aNoTransition) {
        // Force a layout flush:
        aItem.getBoundingClientRect();
        aItem.removeAttribute("notransition");
      }
    } else {
      aItem.removeAttribute("dragover");
      if (aNextItem) {
        if (nextArea == currentArea) {
          // No need to do anything if we're still dragging in this area:
          return;
        }
      }
      // Otherwise, clear everything out:
      let positionManager = DragPositionManager.getManagerForArea(currentArea);
      positionManager.clearPlaceholders(currentArea, aNoTransition);
    }
  },

  _setGridDragActive(aDragOverNode, aDraggedItem, aValue) {
    let targetArea = this._getCustomizableParent(aDragOverNode);
    let draggedWrapper = this.$("wrapper-" + aDraggedItem.id);
    let originArea = this._getCustomizableParent(draggedWrapper);
    let positionManager = DragPositionManager.getManagerForArea(targetArea);
    let draggedSize = this._getDragItemSize(aDragOverNode, aDraggedItem);
    positionManager.insertPlaceholder(
      targetArea,
      aDragOverNode,
      draggedSize,
      originArea == targetArea
    );
  },

  _getDragItemSize(aDragOverNode, aDraggedItem) {
    // Cache it good, cache it real good.
    if (!this._dragSizeMap) {
      this._dragSizeMap = new WeakMap();
    }
    if (!this._dragSizeMap.has(aDraggedItem)) {
      this._dragSizeMap.set(aDraggedItem, new WeakMap());
    }
    let itemMap = this._dragSizeMap.get(aDraggedItem);
    let targetArea = this._getCustomizableParent(aDragOverNode);
    let currentArea = this._getCustomizableParent(aDraggedItem);
    // Return the size for this target from cache, if it exists.
    let size = itemMap.get(targetArea);
    if (size) {
      return size;
    }

    // Calculate size of the item when it'd be dropped in this position.
    let currentParent = aDraggedItem.parentNode;
    let currentSibling = aDraggedItem.nextElementSibling;
    const kAreaType = "cui-areatype";
    let areaType, currentType;

    if (targetArea != currentArea) {
      // Move the widget temporarily next to the placeholder.
      aDragOverNode.parentNode.insertBefore(aDraggedItem, aDragOverNode);
      // Update the node's areaType.
      areaType = CustomizableUI.getAreaType(targetArea.id);
      currentType =
        aDraggedItem.hasAttribute(kAreaType) &&
        aDraggedItem.getAttribute(kAreaType);
      if (areaType) {
        aDraggedItem.setAttribute(kAreaType, areaType);
      }
      this.wrapToolbarItem(aDraggedItem, areaType || "palette");
      CustomizableUI.onWidgetDrag(aDraggedItem.id, targetArea.id);
    } else {
      aDraggedItem.parentNode.hidden = false;
    }

    // Fetch the new size.
    let rect = aDraggedItem.parentNode.getBoundingClientRect();
    size = { width: rect.width, height: rect.height };
    // Cache the found value of size for this target.
    itemMap.set(targetArea, size);

    if (targetArea != currentArea) {
      this.unwrapToolbarItem(aDraggedItem.parentNode);
      // Put the item back into its previous position.
      currentParent.insertBefore(aDraggedItem, currentSibling);
      // restore the areaType
      if (areaType) {
        if (currentType === false) {
          aDraggedItem.removeAttribute(kAreaType);
        } else {
          aDraggedItem.setAttribute(kAreaType, currentType);
        }
      }
      this.createOrUpdateWrapper(aDraggedItem, null, true);
      CustomizableUI.onWidgetDrag(aDraggedItem.id);
    } else {
      aDraggedItem.parentNode.hidden = true;
    }
    return size;
  },

  _getCustomizableParent(aElement) {
    if (aElement) {
      // Deal with drag/drop on the padding of the panel.
      let containingPanelHolder = aElement.closest(
        "#customization-panelHolder"
      );
      if (containingPanelHolder) {
        return containingPanelHolder.querySelector(
          "#widget-overflow-fixed-list"
        );
      }
    }

    let areas = CustomizableUI.areas;
    areas.push(kPaletteId);
    return aElement.closest(areas.map(a => "#" + CSS.escape(a)).join(","));
  },

  _getDragOverNode(aEvent, aAreaElement, aAreaType, aDraggedItemId) {
    let expectedParent =
      CustomizableUI.getCustomizationTarget(aAreaElement) || aAreaElement;
    if (!expectedParent.contains(aEvent.target)) {
      return expectedParent;
    }
    // Offset the drag event's position with the offset to the center of
    // the thing we're dragging
    let dragX = aEvent.clientX - this._dragOffset.x;
    let dragY = aEvent.clientY - this._dragOffset.y;

    // Ensure this is within the container
    let boundsContainer = expectedParent;
    let bounds = this._getBoundsWithoutFlushing(boundsContainer);
    dragX = Math.min(bounds.right, Math.max(dragX, bounds.left));
    dragY = Math.min(bounds.bottom, Math.max(dragY, bounds.top));

    let targetNode;
    if (aAreaType == "toolbar" || aAreaType == "menu-panel") {
      targetNode = aAreaElement.ownerDocument.elementFromPoint(dragX, dragY);
      while (targetNode && targetNode.parentNode != expectedParent) {
        targetNode = targetNode.parentNode;
      }
    } else {
      let positionManager = DragPositionManager.getManagerForArea(aAreaElement);
      // Make it relative to the container:
      dragX -= bounds.left;
      dragY -= bounds.top;
      // Find the closest node:
      targetNode = positionManager.find(aAreaElement, dragX, dragY);
    }
    return targetNode || aEvent.target;
  },

  _onMouseDown(aEvent) {
    log.debug("_onMouseDown");
    if (aEvent.button != 0) {
      return;
    }
    let doc = aEvent.target.ownerDocument;
    doc.documentElement.setAttribute("customizing-movingItem", true);
    let item = this._getWrapper(aEvent.target);
    if (item) {
      item.setAttribute("mousedown", "true");
    }
  },

  _onMouseUp(aEvent) {
    log.debug("_onMouseUp");
    if (aEvent.button != 0) {
      return;
    }
    let doc = aEvent.target.ownerDocument;
    doc.documentElement.removeAttribute("customizing-movingItem");
    let item = this._getWrapper(aEvent.target);
    if (item) {
      item.removeAttribute("mousedown");
    }
  },

  _getWrapper(aElement) {
    while (aElement && aElement.localName != "toolbarpaletteitem") {
      if (aElement.localName == "toolbar") {
        return null;
      }
      aElement = aElement.parentNode;
    }
    return aElement;
  },

  _findVisiblePreviousSiblingNode(aReferenceNode) {
    while (
      aReferenceNode &&
      aReferenceNode.localName == "toolbarpaletteitem" &&
      aReferenceNode.firstElementChild.hidden
    ) {
      aReferenceNode = aReferenceNode.previousElementSibling;
    }
    return aReferenceNode;
  },

  onPaletteContextMenuShowing(event) {
    let isFlexibleSpace = event.target.triggerNode.id.includes(
      "wrapper-customizableui-special-spring"
    );
    event.target.querySelector(
      ".customize-context-addToPanel"
    ).disabled = isFlexibleSpace;
  },

  onPanelContextMenuShowing(event) {
    let inPermanentArea = !!event.target.triggerNode.closest(
      "#widget-overflow-fixed-list"
    );
    let doc = event.target.ownerDocument;
    doc.getElementById(
      "customizationPanelItemContextMenuUnpin"
    ).hidden = !inPermanentArea;
    doc.getElementById(
      "customizationPanelItemContextMenuPin"
    ).hidden = inPermanentArea;

    doc.ownerGlobal.MozXULElement.insertFTLIfNeeded(
      "browser/toolbarContextMenu.ftl"
    );
    event.target.querySelectorAll("[data-lazy-l10n-id]").forEach(el => {
      el.setAttribute("data-l10n-id", el.getAttribute("data-lazy-l10n-id"));
      el.removeAttribute("data-lazy-l10n-id");
    });
  },

  _checkForDownloadsClick(event) {
    if (
      event.target.closest("#wrapper-downloads-button") &&
      event.button == 0
    ) {
      event.view.gCustomizeMode._showDownloadsAutoHidePanel();
    }
  },

  _setupDownloadAutoHideToggle() {
    this.window.addEventListener("click", this._checkForDownloadsClick, true);
  },

  _teardownDownloadAutoHideToggle() {
    this.window.removeEventListener(
      "click",
      this._checkForDownloadsClick,
      true
    );
    this.$(kDownloadAutohidePanelId).hidePopup();
  },

  _maybeMoveDownloadsButtonToNavBar() {
    // If the user toggled the autohide checkbox while the item was in the
    // palette, and hasn't moved it since, move the item to the default
    // location in the navbar for them.
    if (
      !CustomizableUI.getPlacementOfWidget("downloads-button") &&
      this._moveDownloadsButtonToNavBar &&
      this.window.DownloadsButton.autoHideDownloadsButton
    ) {
      let navbarPlacements = CustomizableUI.getWidgetIdsInArea("nav-bar");
      let insertionPoint = navbarPlacements.indexOf("urlbar-container");
      while (++insertionPoint < navbarPlacements.length) {
        let widget = navbarPlacements[insertionPoint];
        // If we find a non-searchbar, non-spacer node, break out of the loop:
        if (
          widget != "search-container" &&
          !(CustomizableUI.isSpecialWidget(widget) && widget.includes("spring"))
        ) {
          break;
        }
      }
      CustomizableUI.addWidgetToArea(
        "downloads-button",
        "nav-bar",
        insertionPoint
      );
      BrowserUsageTelemetry.recordWidgetChange(
        "downloads-button",
        "nav-bar",
        "move-downloads"
      );
    }
  },

  async _showDownloadsAutoHidePanel() {
    let doc = this.document;
    let panel = doc.getElementById(kDownloadAutohidePanelId);
    panel.hidePopup();
    let button = doc.getElementById("downloads-button");
    // We don't show the tooltip if the button is in the panel.
    if (button.closest("#widget-overflow-fixed-list")) {
      return;
    }

    let offsetX = 0,
      offsetY = 0;
    let panelOnTheLeft = false;
    let toolbarContainer = button.closest("toolbar");
    if (toolbarContainer && toolbarContainer.id == "nav-bar") {
      let navbarWidgets = CustomizableUI.getWidgetIdsInArea("nav-bar");
      if (
        navbarWidgets.indexOf("urlbar-container") <=
        navbarWidgets.indexOf("downloads-button")
      ) {
        panelOnTheLeft = true;
      }
    } else {
      await this.window.promiseDocumentFlushed(() => {});

      if (!this._customizing || !this._wantToBeInCustomizeMode) {
        return;
      }
      let buttonBounds = this._getBoundsWithoutFlushing(button);
      let windowBounds = this._getBoundsWithoutFlushing(doc.documentElement);
      panelOnTheLeft =
        buttonBounds.left + buttonBounds.width / 2 > windowBounds.width / 2;
    }
    let position;
    if (panelOnTheLeft) {
      // Tested in RTL, these get inverted automatically, so this does the
      // right thing without taking RTL into account explicitly.
      position = "leftcenter topright";
      if (toolbarContainer) {
        offsetX = 8;
      }
    } else {
      position = "rightcenter topleft";
      if (toolbarContainer) {
        offsetX = -8;
      }
    }

    let checkbox = doc.getElementById(kDownloadAutohideCheckboxId);
    if (this.window.DownloadsButton.autoHideDownloadsButton) {
      checkbox.setAttribute("checked", "true");
    } else {
      checkbox.removeAttribute("checked");
    }

    // We don't use the icon to anchor because it might be resizing because of
    // the animations for drag/drop. Hence the use of offsets.
    panel.openPopup(button, position, offsetX, offsetY);
  },

  onDownloadsAutoHideChange(event) {
    let checkbox = event.target.ownerDocument.getElementById(
      kDownloadAutohideCheckboxId
    );
    Services.prefs.setBoolPref(kDownloadAutoHidePref, checkbox.checked);
    // Ensure we move the button (back) after the user leaves customize mode.
    event.view.gCustomizeMode._moveDownloadsButtonToNavBar = checkbox.checked;
  },

  customizeTouchBar() {
    let updater = Cc["@mozilla.org/widget/touchbarupdater;1"].getService(
      Ci.nsITouchBarUpdater
    );
    updater.enterCustomizeMode();
  },

  togglePong(enabled) {
    // It's possible we're toggling for a reason other than hitting
    // the button (we might be exiting, for example), so make sure that
    // the state and checkbox are in sync.
    let whimsyButton = this.$("whimsy-button");
    whimsyButton.checked = enabled;

    if (enabled) {
      this.visiblePalette.setAttribute("whimsypong", "true");
      this.pongArena.hidden = false;
      if (!this.uninitWhimsy) {
        this.uninitWhimsy = this.whimsypong();
      }
    } else {
      this.visiblePalette.removeAttribute("whimsypong");
      if (this.uninitWhimsy) {
        this.uninitWhimsy();
        this.uninitWhimsy = null;
      }
      this.pongArena.hidden = true;
    }
  },

  whimsypong() {
    function update() {
      updateBall();
      updatePlayers();
    }

    function updateBall() {
      if (ball[1] <= 0 || ball[1] >= gameSide) {
        if (
          (ball[1] <= 0 && (ball[0] < p1 || ball[0] > p1 + paddleWidth)) ||
          (ball[1] >= gameSide && (ball[0] < p2 || ball[0] > p2 + paddleWidth))
        ) {
          updateScore(ball[1] <= 0 ? 0 : 1);
        } else {
          if (
            (ball[1] <= 0 &&
              (ball[0] - p1 < paddleEdge ||
                p1 + paddleWidth - ball[0] < paddleEdge)) ||
            (ball[1] >= gameSide &&
              (ball[0] - p2 < paddleEdge ||
                p2 + paddleWidth - ball[0] < paddleEdge))
          ) {
            ballDxDy[0] *= Math.random() + 1.3;
            ballDxDy[0] = Math.max(Math.min(ballDxDy[0], 6), -6);
            if (Math.abs(ballDxDy[0]) == 6) {
              ballDxDy[0] += Math.sign(ballDxDy[0]) * Math.random();
            }
          } else {
            ballDxDy[0] /= 1.1;
          }
          ballDxDy[1] *= -1;
          ball[1] = ball[1] <= 0 ? 0 : gameSide;
        }
      }
      ball = [
        Math.max(Math.min(ball[0] + ballDxDy[0], gameSide), 0),
        Math.max(Math.min(ball[1] + ballDxDy[1], gameSide), 0),
      ];
      if (ball[0] <= 0 || ball[0] >= gameSide) {
        ballDxDy[0] *= -1;
      }
    }

    function updatePlayers() {
      if (keydown) {
        let p1Adj = 1;
        if (
          (keydown == 37 && !window.RTL_UI) ||
          (keydown == 39 && window.RTL_UI)
        ) {
          p1Adj = -1;
        }
        p1 += p1Adj * 10 * keydownAdj;
      }

      let sign = Math.sign(ballDxDy[0]);
      if (
        (sign > 0 && ball[0] > p2 + paddleWidth / 2) ||
        (sign < 0 && ball[0] < p2 + paddleWidth / 2)
      ) {
        p2 += sign * 3;
      } else if (
        (sign > 0 && ball[0] > p2 + paddleWidth / 1.1) ||
        (sign < 0 && ball[0] < p2 + paddleWidth / 1.1)
      ) {
        p2 += sign * 9;
      }

      if (score >= winScore) {
        p1 = ball[0];
        p2 = ball[0];
      }
      p1 = Math.max(Math.min(p1, gameSide - paddleWidth), 0);
      p2 = Math.max(Math.min(p2, gameSide - paddleWidth), 0);
    }

    function updateScore(adj) {
      if (adj) {
        score += adj;
      } else if (--lives == 0) {
        quit = true;
      }
      ball = ballDef.slice();
      ballDxDy = ballDxDyDef.slice();
      ballDxDy[1] *= score / winScore + 1;
    }

    function draw() {
      let xAdj = window.RTL_UI ? -1 : 1;
      elements["wp-player1"].style.transform =
        "translate(" + xAdj * p1 + "px, -37px)";
      elements["wp-player2"].style.transform =
        "translate(" + xAdj * p2 + "px, " + gameSide + "px)";
      elements["wp-ball"].style.transform =
        "translate(" + xAdj * ball[0] + "px, " + ball[1] + "px)";
      elements["wp-score"].textContent = score;
      elements["wp-lives"].setAttribute("lives", lives);
      if (score >= winScore) {
        let arena = elements.arena;
        let image = "url(chrome://browser/skin/customizableui/whimsy.png)";
        let position = `${(window.RTL_UI ? gameSide : 0) +
          xAdj * ball[0] -
          10}px ${ball[1] - 10}px`;
        let repeat = "no-repeat";
        let size = "20px";
        if (arena.style.backgroundImage) {
          if (arena.style.backgroundImage.split(",").length >= 160) {
            quit = true;
          }

          image += ", " + arena.style.backgroundImage;
          position += ", " + arena.style.backgroundPosition;
          repeat += ", " + arena.style.backgroundRepeat;
          size += ", " + arena.style.backgroundSize;
        }
        arena.style.backgroundImage = image;
        arena.style.backgroundPosition = position;
        arena.style.backgroundRepeat = repeat;
        arena.style.backgroundSize = size;
      }
    }

    function onkeydown(event) {
      keys.push(event.which);
      if (keys.length > 10) {
        keys.shift();
        let codeEntered = true;
        for (let i = 0; i < keys.length; i++) {
          if (keys[i] != keysCode[i]) {
            codeEntered = false;
            break;
          }
        }
        if (codeEntered) {
          elements.arena.setAttribute("kcode", "true");
          let spacer = document.querySelector(
            "#customization-palette > toolbarpaletteitem"
          );
          spacer.setAttribute("kcode", "true");
        }
      }
      if (event.which == 37 /* left */ || event.which == 39 /* right */) {
        keydown = event.which;
        keydownAdj *= 1.05;
      }
    }

    function onkeyup(event) {
      if (event.which == 37 || event.which == 39) {
        keydownAdj = 1;
        keydown = 0;
      }
    }

    function uninit() {
      document.removeEventListener("keydown", onkeydown);
      document.removeEventListener("keyup", onkeyup);
      if (rAFHandle) {
        window.cancelAnimationFrame(rAFHandle);
      }
      let arena = elements.arena;
      while (arena.firstChild) {
        arena.firstChild.remove();
      }
      arena.removeAttribute("score");
      arena.removeAttribute("lives");
      arena.removeAttribute("kcode");
      arena.style.removeProperty("background-image");
      arena.style.removeProperty("background-position");
      arena.style.removeProperty("background-repeat");
      arena.style.removeProperty("background-size");
      let spacer = document.querySelector(
        "#customization-palette > toolbarpaletteitem"
      );
      spacer.removeAttribute("kcode");
      elements = null;
      document = null;
      quit = true;
    }

    if (this.uninitWhimsy) {
      return this.uninitWhimsy;
    }

    let ballDef = [10, 10];
    let ball = [10, 10];
    let ballDxDyDef = [2, 2];
    let ballDxDy = [2, 2];
    let score = 0;
    let p1 = 0;
    let p2 = 10;
    let gameSide = 300;
    let paddleEdge = 30;
    let paddleWidth = 84;
    let keydownAdj = 1;
    let keydown = 0;
    let keys = [];
    let keysCode = [38, 38, 40, 40, 37, 39, 37, 39, 66, 65];
    let lives = 5;
    let winScore = 11;
    let quit = false;
    let document = this.document;
    let rAFHandle = 0;
    let elements = {
      arena: document.getElementById("customization-pong-arena"),
    };

    document.addEventListener("keydown", onkeydown);
    document.addEventListener("keyup", onkeyup);

    for (let id of ["player1", "player2", "ball", "score", "lives"]) {
      let el = document.createXULElement("box");
      el.id = "wp-" + id;
      elements[el.id] = elements.arena.appendChild(el);
    }

    let spacer = this.visiblePalette.querySelector("toolbarpaletteitem");
    for (let player of ["#wp-player1", "#wp-player2"]) {
      let val = "-moz-element(#" + spacer.id + ") no-repeat";
      elements.arena.querySelector(player).style.background = val;
    }

    let window = this.window;
    rAFHandle = window.requestAnimationFrame(function animate() {
      update();
      draw();
      if (quit) {
        elements["wp-score"].textContent = score;
        elements["wp-lives"] &&
          elements["wp-lives"].setAttribute("lives", lives);
        elements.arena.setAttribute("score", score);
        elements.arena.setAttribute("lives", lives);
      } else {
        rAFHandle = window.requestAnimationFrame(animate);
      }
    });

    return uninit;
  },
};

function __dumpDragData(aEvent, caller) {
  if (!gDebug) {
    return;
  }
  let str =
    "Dumping drag data (" +
    (caller ? caller + " in " : "") +
    "CustomizeMode.jsm) {\n";
  str += "  type: " + aEvent.type + "\n";
  for (let el of ["target", "currentTarget", "relatedTarget"]) {
    if (aEvent[el]) {
      str +=
        "  " +
        el +
        ": " +
        aEvent[el] +
        "(localName=" +
        aEvent[el].localName +
        "; id=" +
        aEvent[el].id +
        ")\n";
    }
  }
  for (let prop in aEvent.dataTransfer) {
    if (typeof aEvent.dataTransfer[prop] != "function") {
      str +=
        "  dataTransfer[" + prop + "]: " + aEvent.dataTransfer[prop] + "\n";
    }
  }
  str += "}";
  log.debug(str);
}

function dispatchFunction(aFunc) {
  Services.tm.dispatchToMainThread(aFunc);
}
