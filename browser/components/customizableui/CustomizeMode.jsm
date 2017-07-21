/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["CustomizeMode"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

const kPrefCustomizationDebug = "browser.uiCustomization.debug";
const kPrefCustomizationAnimation = "browser.uiCustomization.disableAnimation";
const kPaletteId = "customization-palette";
const kDragDataTypePrefix = "text/toolbarwrapper-id/";
const kPlaceholderClass = "panel-customization-placeholder";
const kSkipSourceNodePref = "browser.uiCustomization.skipSourceNodeCheck";
const kToolbarVisibilityBtn = "customization-toolbar-visibility-button";
const kDrawInTitlebarPref = "browser.tabs.drawInTitlebar";
const kMaxTransitionDurationMs = 2000;

const kPanelItemContextMenu = "customizationPanelItemContextMenu";
const kPaletteItemContextMenu = "customizationPaletteItemContextMenu";

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/CustomizableUI.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/AddonManager.jsm");
Cu.import("resource://gre/modules/AppConstants.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "DragPositionManager",
                                  "resource:///modules/DragPositionManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "BrowserUITelemetry",
                                  "resource:///modules/BrowserUITelemetry.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LightweightThemeManager",
                                  "resource://gre/modules/LightweightThemeManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "SessionStore",
                                  "resource:///modules/sessionstore/SessionStore.jsm");
XPCOMUtils.defineLazyGetter(this, "gWidgetsBundle", function() {
  const kUrl = "chrome://browser/locale/customizableui/customizableWidgets.properties";
  return Services.strings.createBundle(kUrl);
});

XPCOMUtils.defineLazyPreferenceGetter(this, "gPhotonStructure",
  "browser.photon.structure.enabled", false);

let gDebug;
XPCOMUtils.defineLazyGetter(this, "log", () => {
  let scope = {};
  Cu.import("resource://gre/modules/Console.jsm", scope);
  gDebug = Services.prefs.getBoolPref(kPrefCustomizationDebug, false);
  let consoleOptions = {
    maxLogLevel: gDebug ? "all" : "log",
    prefix: "CustomizeMode",
  };
  return new scope.ConsoleAPI(consoleOptions);
});

var gDisableAnimation = null;

var gDraggingInToolbars;

var gTab;

function closeGlobalTab() {
  let win = gTab.ownerGlobal;
  if (win.gBrowser.browsers.length == 1) {
    win.BrowserOpenTab();
  }
  win.gBrowser.removeTab(gTab);
  gTab = null;
}

var gTabsProgressListener = {
  onLocationChange(aBrowser, aWebProgress, aRequest, aLocation, aFlags) {
    if (!gTab || gTab.linkedBrowser != aBrowser) {
      return;
    }

    unregisterGlobalTab();
  },
}

function unregisterGlobalTab() {
  gTab.removeEventListener("TabClose", unregisterGlobalTab);
  let win = gTab.ownerGlobal;
  win.removeEventListener("unload", unregisterGlobalTab);
  win.gBrowser.removeTabsProgressListener(gTabsProgressListener);

  gTab.removeAttribute("customizemode");

  gTab = null;
}

function CustomizeMode(aWindow) {
  if (gDisableAnimation === null) {
    gDisableAnimation = Services.prefs.getPrefType(kPrefCustomizationAnimation) == Ci.nsIPrefBranch.PREF_BOOL &&
                        Services.prefs.getBoolPref(kPrefCustomizationAnimation);
  }
  this.window = aWindow;
  this.document = aWindow.document;
  this.browser = aWindow.gBrowser;
  this.areas = new Set();

  // There are two palettes - there's the palette that can be overlayed with
  // toolbar items in browser.xul. This is invisible, and never seen by the
  // user. Then there's the visible palette, which gets populated and displayed
  // to the user when in customizing mode.
  this.visiblePalette = this.document.getElementById(kPaletteId);
  this.paletteEmptyNotice = this.document.getElementById("customization-empty");
  this.tipPanel = this.document.getElementById("customization-tipPanel");
  if (Services.prefs.getCharPref("general.skins.selectedSkin") != "classic/1.0") {
    let lwthemeButton = this.document.getElementById("customization-lwtheme-button");
    lwthemeButton.setAttribute("hidden", "true");
  }
  if (AppConstants.CAN_DRAW_IN_TITLEBAR) {
    this._updateTitlebarCheckbox();
    Services.prefs.addObserver(kDrawInTitlebarPref, this);
  }
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

  get panelUIContents() {
    return this.document.getElementById("PanelUI-contents");
  },

  get _handler() {
    return this.window.CustomizationHandler;
  },

  uninit() {
    if (AppConstants.CAN_DRAW_IN_TITLEBAR) {
      Services.prefs.removeObserver(kDrawInTitlebarPref, this);
    }
  },

  toggle() {
    if (this._handler.isEnteringCustomizeMode || this._handler.isExitingCustomizeMode) {
      this._wantToBeInCustomizeMode = !this._wantToBeInCustomizeMode;
      return;
    }
    if (this._customizing) {
      this.exit();
    } else {
      this.enter();
    }
  },

  _updateLWThemeButtonIcon() {
    let lwthemeButton = this.document.getElementById("customization-lwtheme-button");
    let lwthemeIcon = this.document.getAnonymousElementByAttribute(lwthemeButton,
                        "class", "button-icon");
    lwthemeIcon.style.backgroundImage = LightweightThemeManager.currentTheme ?
      "url(" + LightweightThemeManager.currentTheme.iconURL + ")" : "";
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

    gTab.linkedBrowser.stop();

    let win = gTab.ownerGlobal;

    win.gBrowser.setTabTitle(gTab);
    win.gBrowser.setIcon(gTab,
                         "chrome://browser/skin/customizableui/customizeFavicon.ico");

    gTab.addEventListener("TabClose", unregisterGlobalTab);

    win.gBrowser.addTabsProgressListener(gTabsProgressListener);

    win.addEventListener("unload", unregisterGlobalTab);

    if (gTab.selected) {
      win.gCustomizeMode.enter();
    }
  },

  enter() {
    this._wantToBeInCustomizeMode = true;

    if (this._customizing || this._handler.isEnteringCustomizeMode) {
      return;
    }

    // Exiting; want to re-enter once we've done that.
    if (this._handler.isExitingCustomizeMode) {
      log.debug("Attempted to enter while we're in the middle of exiting. " +
                "We'll exit after we've entered");
      return;
    }

    if (!gTab) {
      this.setTab(this.browser.loadOneTab("about:blank", {
        inBackground: false,
        forceNotRemote: true,
        skipAnimation: true,
        triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      }));
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
    let resetButton = this.document.getElementById("customization-reset-button");
    resetButton.setAttribute("disabled", "true");

    (async () => {
      // We shouldn't start customize mode until after browser-delayed-startup has finished:
      if (!this.window.gBrowserInit.delayedStartupFinished) {
        await new Promise(resolve => {
          let delayedStartupObserver = aSubject => {
            if (aSubject == this.window) {
              Services.obs.removeObserver(delayedStartupObserver, "browser-delayed-startup-finished");
              resolve();
            }
          };

          Services.obs.addObserver(delayedStartupObserver, "browser-delayed-startup-finished");
        });
      }

      let toolbarVisibilityBtn = document.getElementById(kToolbarVisibilityBtn);
      let togglableToolbars = window.getTogglableToolbars();
      if (togglableToolbars.length == 0) {
        toolbarVisibilityBtn.setAttribute("hidden", "true");
      } else {
        toolbarVisibilityBtn.removeAttribute("hidden");
      }

      this.updateLWTStyling();

      CustomizableUI.dispatchToolboxEvent("beforecustomization", {}, window);
      CustomizableUI.notifyStartCustomizing(this.window);

      // Add a keypress listener to the document so that we can quickly exit
      // customization mode when pressing ESC.
      document.addEventListener("keypress", this);

      // Same goes for the menu button - if we're customizing, a click on the
      // menu button means a quick exit from customization mode.
      window.PanelUI.hide();

      let panelHolder = document.getElementById("customization-panelHolder");
      let panelContainer = document.getElementById("customization-panel-container");
      let customizationContainer = document.getElementById("customization-container");
      let paletteContainer = document.getElementById("customization-palette-container");
      let contentContainer = document.getElementById("customization-content-container");
      let footer = document.getElementById("customization-footer");
      let doneButton = document.getElementById("customization-done-button");
      let panelContextMenu = document.getElementById(kPanelItemContextMenu);
      this._previousPanelContextMenuParent = panelContextMenu.parentNode;
      document.getElementById("mainPopupSet").appendChild(panelContextMenu);
      if (gPhotonStructure) {
        if (!customizationContainer.hasAttribute("photon")) {
          contentContainer.appendChild(paletteContainer);
          contentContainer.appendChild(panelContainer);
          customizationContainer.appendChild(footer);
          customizationContainer.setAttribute("photon", "true");
          doneButton.hidden = false;
        }
        panelHolder.appendChild(window.PanelUI.overflowFixedList);
        window.PanelUI.overflowFixedList.setAttribute("customizing", true);
        window.PanelUI.menuButton.disabled = true;
        document.getElementById("nav-bar-overflow-button").disabled = true;
      } else {
        if (customizationContainer.hasAttribute("photon")) {
          customizationContainer.insertBefore(paletteContainer, contentContainer);
          customizationContainer.appendChild(panelContainer);
          paletteContainer.appendChild(footer);
          customizationContainer.removeAttribute("photon");
          doneButton.hidden = true;
        }
        window.PanelUI.menuButton.addEventListener("command", this);
        window.PanelUI.menuButton.open = true;
        window.PanelUI.beginBatchUpdate();

        // The menu panel is lazy, and registers itself when the popup shows.
        // If it hasn't been opened yet, we need to force the menu panel to
        // register itself, or else customization is not going to work.
        // We pass "true" to ensureReady to indicate that we're handling
        // calling startBatchUpdate and endBatchUpdate.
        if (!window.PanelUI.isReady) {
          await window.PanelUI.ensureReady(true);
          // Up to now, it will have been hidden, and its XBL bindings won't have
          // been constructed. Unhiding it won't trigger the construction of
          // those bindings immediately, but the next layout flush will.
          // Because we don't want to sync flush layout, we wait for the next
          // natural style/layout flush.
          await new Promise(resolve => window.requestIdleCallback(resolve));
        }

        // Hide the palette before starting the transition for increased perf.
        this.visiblePalette.hidden = true;
        this.visiblePalette.removeAttribute("showing");

        if (!AppConstants.MOZ_PHOTON_THEME) {
          // Disable the button-text fade-out mask
          // during the transition for increased perf.
          window.PanelUI.contents.setAttribute("customize-transitioning", "true");
        }

        // Move the mainView in the panel to the holder so that we can see it
        // while customizing.
        let mainView = window.PanelUI.mainView;
        panelHolder.appendChild(mainView);

        let customizeButton = document.getElementById("PanelUI-customize");
        customizeButton.setAttribute("enterLabel", customizeButton.getAttribute("label"));
        customizeButton.setAttribute("label", customizeButton.getAttribute("exitLabel"));
        customizeButton.setAttribute("enterTooltiptext", customizeButton.getAttribute("tooltiptext"));
        customizeButton.setAttribute("tooltiptext", customizeButton.getAttribute("exitTooltiptext"));

        this._mainViewContext = mainView.getAttribute("context");
        if (this._mainViewContext) {
          mainView.removeAttribute("context");
        }
      }

      this._transitioning = true;

      let customizer = document.getElementById("customization-container");
      customizer.parentNode.selectedPanel = customizer;
      customizer.hidden = false;

      this._wrapToolbarItemSync(CustomizableUI.AREA_TABSTRIP);

      let customizableToolbars = document.querySelectorAll("toolbar[customizable=true]:not([autohide=true]):not([collapsed=true])");
      for (let toolbar of customizableToolbars)
        toolbar.setAttribute("customizing", true);

      await this._doTransition(true);

      Services.obs.addObserver(this, "lightweight-theme-window-updated");

      // Let everybody in this window know that we're about to customize.
      CustomizableUI.dispatchToolboxEvent("customizationstarting", {}, window);

      if (!gPhotonStructure) {
        this._showPanelCustomizationPlaceholders();
      }

      await this._wrapToolbarItems();
      this.populatePalette();

      this._addDragHandlers(this.visiblePalette);

      window.gNavToolbox.addEventListener("toolbarvisibilitychange", this);

      if (!gPhotonStructure) {
        document.getElementById("PanelUI-help").setAttribute("disabled", true);
        document.getElementById("PanelUI-quit").setAttribute("disabled", true);
      }

      this._updateResetButton();
      this._updateUndoResetButton();

      this._skipSourceNodeCheck = Services.prefs.getPrefType(kSkipSourceNodePref) == Ci.nsIPrefBranch.PREF_BOOL &&
                                  Services.prefs.getBoolPref(kSkipSourceNodePref);

      CustomizableUI.addListener(this);
      if (!gPhotonStructure) {
        window.PanelUI.endBatchUpdate();
      }
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

      this._updateLWThemeButtonIcon();
      if (!AppConstants.MOZ_PHOTON_THEME) {
        this.maybeShowTip(panelHolder);
      }

      this._handler.isEnteringCustomizeMode = false;
      if (!gPhotonStructure) {
        window.PanelUI.contents.removeAttribute("customize-transitioning");
      }

      CustomizableUI.dispatchToolboxEvent("customizationready", {}, window);
      this._enableOutlinesTimeout = window.setTimeout(() => {
        this.document.getElementById("nav-bar").setAttribute("showoutline", "true");
        if (!gPhotonStructure) {
          this.panelUIContents.setAttribute("showoutline", "true");
        }
        delete this._enableOutlinesTimeout;
      }, 0);

      if (!this._wantToBeInCustomizeMode) {
        this.exit();
      }
    })().catch(e => {
      log.error("Error entering customize mode", e);
      // We should ensure this has been called, and calling it again doesn't hurt:
      window.PanelUI.endBatchUpdate();
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
      log.debug("Attempted to exit while we're in the middle of entering. " +
                "We'll exit after we've entered");
      return;
    }

    if (this.resetting) {
      log.debug("Attempted to exit while we're resetting. " +
                "We'll exit after resetting has finished.");
      return;
    }

    this.hideTip();

    this._handler.isExitingCustomizeMode = true;

    if (this._enableOutlinesTimeout) {
      this.window.clearTimeout(this._enableOutlinesTimeout);
    } else {
      this.document.getElementById("nav-bar").removeAttribute("showoutline");
      this.panelUIContents.removeAttribute("showoutline");
    }

    this._removeExtraToolbarsIfEmpty();

    CustomizableUI.removeListener(this);

    this.document.removeEventListener("keypress", this);
    if (!gPhotonStructure) {
      this.window.PanelUI.menuButton.removeEventListener("command", this);
      this.window.PanelUI.menuButton.open = false;

      this.window.PanelUI.beginBatchUpdate();

      this._removePanelCustomizationPlaceholders();
    }

    let window = this.window;
    let document = this.document;

    // Hide the palette before starting the transition for increased perf.
    this.visiblePalette.hidden = true;
    this.visiblePalette.removeAttribute("showing");
    this.paletteEmptyNotice.hidden = true;

    if (!gPhotonStructure) {
      // Disable the button-text fade-out mask
      // during the transition for increased perf.
      let panelContents = window.PanelUI.contents;
      panelContents.setAttribute("customize-transitioning", "true");
    }

    // Disable the reset and undo reset buttons while transitioning:
    let resetButton = this.document.getElementById("customization-reset-button");
    let undoResetButton = this.document.getElementById("customization-undo-reset-button");
    undoResetButton.hidden = resetButton.disabled = true;

    this._transitioning = true;

    (async () => {
      await this.depopulatePalette();

      await this._doTransition(false);
      this.updateLWTStyling({});

      Services.obs.removeObserver(this, "lightweight-theme-window-updated");

      if (this.browser.selectedTab == gTab) {
        if (gTab.linkedBrowser.currentURI.spec == "about:blank") {
          closeGlobalTab();
        } else {
          unregisterGlobalTab();
        }
      }
      let browser = document.getElementById("browser");
      browser.parentNode.selectedPanel = browser;
      let customizer = document.getElementById("customization-container");
      customizer.hidden = true;

      window.gNavToolbox.removeEventListener("toolbarvisibilitychange", this);

      DragPositionManager.stop();
      this._removeDragHandlers(this.visiblePalette);

      await this._unwrapToolbarItems();

      if (this._changed) {
        // XXXmconley: At first, it seems strange to also persist the old way with
        //             currentset - but this might actually be useful for switching
        //             to old builds. We might want to keep this around for a little
        //             bit.
        this.persistCurrentSets();
      }

      // And drop all area references.
      this.areas.clear();

      // Let everybody in this window know that we're starting to
      // exit customization mode.
      CustomizableUI.dispatchToolboxEvent("customizationending", {}, window);

      window.PanelUI.menuButton.disabled = false;
      if (gPhotonStructure) {
        let overflowContainer = document.getElementById("widget-overflow-scroller");
        overflowContainer.appendChild(window.PanelUI.overflowFixedList);
        document.getElementById("nav-bar-overflow-button").disabled = false;
      } else {
        window.PanelUI.setMainView(window.PanelUI.mainView);

        let customizeButton = document.getElementById("PanelUI-customize");
        customizeButton.setAttribute("exitLabel", customizeButton.getAttribute("label"));
        customizeButton.setAttribute("label", customizeButton.getAttribute("enterLabel"));
        customizeButton.setAttribute("exitTooltiptext", customizeButton.getAttribute("tooltiptext"));
        customizeButton.setAttribute("tooltiptext", customizeButton.getAttribute("enterTooltiptext"));

        // We have to use setAttribute/removeAttribute here instead of the
        // property because the XBL property will be set later, and right
        // now we'd be setting an expando, which breaks the XBL property.
        document.getElementById("PanelUI-help").removeAttribute("disabled");
        document.getElementById("PanelUI-quit").removeAttribute("disabled");
        this.panelUIContents.removeAttribute("customize-transitioning");
      }
      let panelContextMenu = document.getElementById(kPanelItemContextMenu);
      this._previousPanelContextMenuParent.appendChild(panelContextMenu);

      // We need to set this._customizing to false before removing the tab
      // or the TabSelect event handler will think that we are exiting
      // customization mode for a second time.
      this._customizing = false;

      if (!gPhotonStructure) {
        let mainView = window.PanelUI.mainView;
        if (this._mainViewContext) {
          mainView.setAttribute("context", this._mainViewContext);
        }
      }

      let customizableToolbars = document.querySelectorAll("toolbar[customizable=true]:not([autohide=true])");
      for (let toolbar of customizableToolbars)
        toolbar.removeAttribute("customizing");

      if (!gPhotonStructure) {
        this.window.PanelUI.endBatchUpdate();
      }
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
      if (!gPhotonStructure) {
        // We should ensure this has been called, and calling it again doesn't hurt:
        window.PanelUI.endBatchUpdate();
      }
      this._handler.isExitingCustomizeMode = false;
    });
  },

  /**
   * The customize mode transition has 4 phases when entering:
   * 1) Pre-customization mode
   *    This is the starting phase of the browser.
   * 2) LWT swapping
   *    This is where we swap some of the lightweight theme styles in order
   *    to make them work in customize mode. We set/unset a customization-
   *    lwtheme attribute iff we're using a lightweight theme.
   * 3) customize-entering
   *    This phase is a transition, optimized for smoothness.
   * 4) customize-entered
   *    After the transition completes, this phase draws all of the
   *    expensive detail that isn't necessary during the second phase.
   *
   * Exiting customization mode has a similar set of phases, but in reverse
   * order - customize-entered, customize-exiting, remove LWT swapping,
   * pre-customization mode.
   *
   * When in the customize-entering, customize-entered, or customize-exiting
   * phases, there is a "customizing" attribute set on the main-window to simplify
   * excluding certain styles while in any phase of customize mode.
   */
  _doTransition(aEntering) {
    if (AppConstants.MOZ_PHOTON_THEME) {
      let docEl = this.document.documentElement;
      if (aEntering) {
        docEl.setAttribute("customizing", true);
        docEl.setAttribute("customize-entered", true);
      } else {
        docEl.removeAttribute("customizing");
        docEl.removeAttribute("customize-entered");
      }
      return Promise.resolve();
    }
    let deck = this.document.getElementById("content-deck");
    let customizeTransitionEndPromise = new Promise(resolve => {
      let customizeTransitionEnd = (aEvent) => {
        if (aEvent != "timedout" &&
            (aEvent.originalTarget != deck || aEvent.propertyName != "margin-left")) {
          return;
        }
        this.window.clearTimeout(catchAllTimeout);
        // We request an animation frame to do the final stage of the transition
        // to improve perceived performance. (bug 962677)
        this.window.requestAnimationFrame(() => {
          deck.removeEventListener("transitionend", customizeTransitionEnd);

          if (!aEntering) {
            this.document.documentElement.removeAttribute("customize-exiting");
            this.document.documentElement.removeAttribute("customizing");
          } else {
            this.document.documentElement.setAttribute("customize-entered", true);
            this.document.documentElement.removeAttribute("customize-entering");
          }
          CustomizableUI.dispatchToolboxEvent("customization-transitionend", aEntering, this.window);

          resolve();
        });
      };
      deck.addEventListener("transitionend", customizeTransitionEnd);
      let catchAll = () => customizeTransitionEnd("timedout");
      let catchAllTimeout = this.window.setTimeout(catchAll, kMaxTransitionDurationMs);
    });

    if (gDisableAnimation) {
      this.document.getElementById("tab-view-deck").setAttribute("fastcustomizeanimation", true);
    }

    if (aEntering) {
      this.document.documentElement.setAttribute("customizing", true);
      this.document.documentElement.setAttribute("customize-entering", true);
    } else {
      this.document.documentElement.setAttribute("customize-exiting", true);
      this.document.documentElement.removeAttribute("customize-entered");
    }

    return customizeTransitionEndPromise;
  },

  updateLWTStyling(aData) {
    let docElement = this.document.documentElement;
    if (!aData) {
      let lwt = docElement._lightweightTheme;
      aData = lwt.getData();
    }
    let headerURL = aData && aData.headerURL;
    if (!headerURL) {
      docElement.removeAttribute("customization-lwtheme");
      return;
    }
    docElement.setAttribute("customization-lwtheme", "true");

    let deck = this.document.getElementById("tab-view-deck");
    let toolboxRect = this.window.gNavToolbox.getBoundingClientRect();
    let height = toolboxRect.bottom;
    deck.style.setProperty("--toolbox-rect-height", `${height}`);
    deck.style.setProperty("--toolbox-rect-height-with-unit", `${height}px`);
  },

  maybeShowTip(aAnchor) {
    const kShownPref = "browser.customizemode.tip0.shown";
    let shown = Services.prefs.getBoolPref(kShownPref, false);
    if (shown)
      return;

    let anchorNode = aAnchor || this.document.getElementById("customization-panelHolder");
    let messageNode = this.tipPanel.querySelector(".customization-tipPanel-contentMessage");
    if (!messageNode.childElementCount) {
      // Put the tip contents in the popup.
      let bundle = this.document.getElementById("bundle_browser");
      const kLabelClass = "customization-tipPanel-link";
      // eslint-disable-next-line no-unsanitized/property
      messageNode.innerHTML = bundle.getFormattedString("customizeTips.tip0", [
        "<label class=\"customization-tipPanel-em\" value=\"" +
          bundle.getString("customizeTips.tip0.hint") + "\"/>",
        this.document.getElementById("bundle_brand").getString("brandShortName"),
        "<label class=\"" + kLabelClass + " text-link\" value=\"" +
        bundle.getString("customizeTips.tip0.learnMore") + "\"/>"
      ]);

      messageNode.querySelector("." + kLabelClass).addEventListener("click", () => {
        let url = Services.urlFormatter.formatURLPref("browser.customizemode.tip0.learnMoreUrl");
        let browser = this.browser;
        browser.selectedTab = browser.addTab(url);
        this.hideTip();
      });
    }

    this.tipPanel.hidden = false;
    this.tipPanel.openPopup(anchorNode);
    Services.prefs.setBoolPref(kShownPref, true);
  },

  hideTip() {
    this.tipPanel.hidePopup();
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
      let customizationTarget = areaNode && areaNode.customizationTarget;
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
      if (areas.indexOf(parent.id) != -1) {
        return aNode;
      }
      aNode = parent;
    }
    return null;
  },

  addToToolbar(aNode) {
    aNode = this._getCustomizableChildForNode(aNode);
    if (aNode.localName == "toolbarpaletteitem" && aNode.firstChild) {
      aNode = aNode.firstChild;
    }
    CustomizableUI.addWidgetToArea(aNode.id, CustomizableUI.AREA_NAVBAR);
    if (!this._customizing) {
      CustomizableUI.dispatchToolboxEvent("customizationchange");
    }
  },

  addToPanel(aNode) {
    aNode = this._getCustomizableChildForNode(aNode);
    if (aNode.localName == "toolbarpaletteitem" && aNode.firstChild) {
      aNode = aNode.firstChild;
    }
    let panel = gPhotonStructure ? CustomizableUI.AREA_FIXED_OVERFLOW_PANEL
                                 : CustomizableUI.AREA_PANEL;
    CustomizableUI.addWidgetToArea(aNode.id, panel);
    if (!this._customizing) {
      CustomizableUI.dispatchToolboxEvent("customizationchange");
    }

    if (AppConstants.MOZ_PHOTON_ANIMATIONS &&
        Services.prefs.getBoolPref("toolkit.cosmeticAnimations.enabled")) {
      let overflowButton = this.document.getElementById("nav-bar-overflow-button");
      overflowButton.setAttribute("animate", "true");
      overflowButton.addEventListener("animationend", function onAnimationEnd(event) {
        if (event.animationName.startsWith("overflow-animation")) {
          this.setAttribute("fade", "true");
        } else if (event.animationName == "overflow-fade") {
          this.removeEventListener("animationend", onAnimationEnd);
          this.removeAttribute("animate");
          this.removeAttribute("fade");
        }
      });
    }
  },

  removeFromArea(aNode) {
    aNode = this._getCustomizableChildForNode(aNode);
    if (aNode.localName == "toolbarpaletteitem" && aNode.firstChild) {
      aNode = aNode.firstChild;
    }
    CustomizableUI.removeWidgetFromArea(aNode.id);
    if (!this._customizing) {
      CustomizableUI.dispatchToolboxEvent("customizationchange");
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

      if (gPhotonStructure) {
        let flexSpace = CustomizableUI.createSpecialWidget("spring", this.document);
        fragment.appendChild(this.wrapToolbarItem(flexSpace, "palette"));
      }

      this.visiblePalette.appendChild(fragment);
      this._stowedPalette = this.window.gNavToolbox.palette;
      this.window.gNavToolbox.palette = this.visiblePalette;
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
      log.error("Widget with id " + aWidget.id + " does not return a valid node");
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

  depopulatePalette() {
    return (async () => {
      this.visiblePalette.hidden = true;
      let paletteChild = this.visiblePalette.firstChild;
      let nextChild;
      while (paletteChild) {
        nextChild = paletteChild.nextElementSibling;
        let itemId = paletteChild.firstChild.id;
        if (CustomizableUI.isSpecialWidget(itemId)) {
          this.visiblePalette.removeChild(paletteChild);
        } else {
          // XXXunf Currently this doesn't destroy the (now unused) node in the
          //       API provider case. It would be good to do so, but we need to
          //       keep strong refs to it in CustomizableUI (can't iterate of
          //       WeakMaps), and there's the question of what behavior
          //       wrappers should have if consumers keep hold of them.
          let unwrappedPaletteItem =
            await this.deferredUnwrapToolbarItem(paletteChild);
          this._stowedPalette.appendChild(unwrappedPaletteItem);
        }

        paletteChild = nextChild;
      }
      this.visiblePalette.hidden = false;
      this.window.gNavToolbox.palette = this._stowedPalette;
    })().catch(log.error);
  },

  isCustomizableItem(aNode) {
    return aNode.localName == "toolbarbutton" ||
           aNode.localName == "toolbaritem" ||
           aNode.localName == "toolbarseparator" ||
           aNode.localName == "toolbarspring" ||
           aNode.localName == "toolbarspacer";
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

  createOrUpdateWrapper(aNode, aPlace, aIsUpdate) {
    let wrapper;
    if (aIsUpdate && aNode.parentNode && aNode.parentNode.localName == "toolbarpaletteitem") {
      wrapper = aNode.parentNode;
      aPlace = wrapper.getAttribute("place");
    } else {
      wrapper = this.document.createElement("toolbarpaletteitem");
      // "place" is used by toolkit to add the toolbarpaletteitem-palette
      // binding to a toolbarpaletteitem, which gives it a label node for when
      // it's sitting in the palette.
      wrapper.setAttribute("place", aPlace);
    }


    // Ensure the wrapped item doesn't look like it's in any special state, and
    // can't be interactved with when in the customization palette.
    if (aNode.hasAttribute("command")) {
      wrapper.setAttribute("itemcommand", aNode.getAttribute("command"));
      aNode.removeAttribute("command");
    }

    if (aNode.hasAttribute("observes")) {
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

    if (aNode.hasAttribute("label")) {
      wrapper.setAttribute("title", aNode.getAttribute("label"));
      wrapper.setAttribute("tooltiptext", aNode.getAttribute("label"));
    } else if (aNode.hasAttribute("title")) {
      wrapper.setAttribute("title", aNode.getAttribute("title"));
      wrapper.setAttribute("tooltiptext", aNode.getAttribute("title"));
    }

    if (aNode.hasAttribute("flex")) {
      wrapper.setAttribute("flex", aNode.getAttribute("flex"));
    }

    if (aPlace == "panel") {
      if (aNode.classList.contains(CustomizableUI.WIDE_PANEL_CLASS)) {
        wrapper.setAttribute("haswideitem", "true");
      } else if (wrapper.hasAttribute("haswideitem")) {
        wrapper.removeAttribute("haswideitem");
      }
    }

    let removable = aPlace == "palette" || CustomizableUI.isWidgetRemovable(aNode);
    wrapper.setAttribute("removable", removable);

    let contextMenuAttrName = "";
    if (aNode.getAttribute("context")) {
      contextMenuAttrName = "context";
    } else if (aNode.getAttribute("contextmenu")) {
      contextMenuAttrName = "contextmenu";
    }
    let currentContextMenu = aNode.getAttribute(contextMenuAttrName);
    let contextMenuForPlace = aPlace == "panel" ?
                                kPanelItemContextMenu :
                                kPaletteItemContextMenu;
    if (aPlace != "toolbar") {
      wrapper.setAttribute("context", contextMenuForPlace);
    }
    // Only keep track of the menu if it is non-default.
    if (currentContextMenu &&
        currentContextMenu != contextMenuForPlace) {
      aNode.setAttribute("wrapped-context", currentContextMenu);
      aNode.setAttribute("wrapped-contextAttrName", contextMenuAttrName)
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
      wrapper.setAttribute("title", gWidgetsBundle.GetStringFromName(aNode.nodeName + ".label"));
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

    let toolbarItem = aWrapper.firstChild;
    if (!toolbarItem) {
      log.error("no toolbarItem child for " + aWrapper.tagName + "#" + aWrapper.id);
      aWrapper.remove();
      return null;
    }

    if (aWrapper.hasAttribute("itemobserves")) {
      toolbarItem.setAttribute("observes", aWrapper.getAttribute("itemobserves"));
    }

    if (aWrapper.hasAttribute("itemchecked")) {
      toolbarItem.checked = true;
    }

    if (aWrapper.hasAttribute("itemcommand")) {
      let commandID = aWrapper.getAttribute("itemcommand");
      toolbarItem.setAttribute("command", commandID);

      // XXX Bug 309953 - toolbarbuttons aren't in sync with their commands after customizing
      let command = this.document.getElementById(commandID);
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
    } else if (place == "panel") {
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
        await this.deferredWrapToolbarItem(child, CustomizableUI.getPlaceForItem(child)).catch(log.error);
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
        if (this.isCustomizableItem(child) && !this.isWrappedToolbarItem(child)) {
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
    if (gPhotonStructure && aTarget.id == CustomizableUI.AREA_FIXED_OVERFLOW_PANEL) {
      aTarget = this.document.getElementById("customization-panelHolder");
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
    if (gPhotonStructure && aTarget.id == CustomizableUI.AREA_FIXED_OVERFLOW_PANEL) {
      aTarget = this.document.getElementById("customization-panelHolder");
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

  _removeExtraToolbarsIfEmpty() {
    let toolbox = this.window.gNavToolbox;
    for (let child of toolbox.children) {
      if (child.hasAttribute("customindex")) {
        let placements = CustomizableUI.getWidgetIdsInArea(child.id);
        if (!placements.length) {
          CustomizableUI.removeExtraToolbar(child.id);
        }
      }
    }
  },

  persistCurrentSets(aSetBeforePersisting) {
    let document = this.document;
    let toolbars = document.querySelectorAll("toolbar[customizable='true'][currentset]");
    for (let toolbar of toolbars) {
      if (aSetBeforePersisting) {
        let set = toolbar.currentSet;
        toolbar.setAttribute("currentset", set);
      }
      // Persist the currentset attribute directly on hardcoded toolbars.
      document.persist(toolbar.id, "currentset");
    }
  },

  reset() {
    this.resetting = true;
    // Disable the reset button temporarily while resetting:
    let btn = this.document.getElementById("customization-reset-button");
    BrowserUITelemetry.countCustomizationEvent("reset");
    btn.disabled = true;
    return (async () => {
      this._removePanelCustomizationPlaceholders();
      await this.depopulatePalette();
      await this._unwrapToolbarItems();

      CustomizableUI.reset();

      this._updateLWThemeButtonIcon();

      await this._wrapToolbarItems();
      this.populatePalette();

      this.persistCurrentSets(true);

      this._updateResetButton();
      this._updateUndoResetButton();
      this._updateEmptyPaletteNotice();
      this._showPanelCustomizationPlaceholders();
      this.resetting = false;
      if (!this._wantToBeInCustomizeMode) {
        this.exit();
      }
    })().catch(log.error);
  },

  undoReset() {
    this.resetting = true;

    return (async () => {
      this._removePanelCustomizationPlaceholders();
      await this.depopulatePalette();
      await this._unwrapToolbarItems();

      CustomizableUI.undoReset();

      this._updateLWThemeButtonIcon();

      await this._wrapToolbarItems();
      this.populatePalette();

      this.persistCurrentSets(true);

      this._updateResetButton();
      this._updateUndoResetButton();
      this._updateEmptyPaletteNotice();
      this.resetting = false;
    })().catch(log.error);
  },

  _onToolbarVisibilityChange(aEvent) {
    let toolbar = aEvent.target;
    if (aEvent.detail.visible && toolbar.getAttribute("customizable") == "true") {
      toolbar.setAttribute("customizing", "true");
    } else {
      toolbar.removeAttribute("customizing");
    }
    this._onUIChange();
    this.updateLWTStyling();
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
    if (aContainer.id == CustomizableUI.AREA_PANEL) {
      this._removePanelCustomizationPlaceholders();
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
    if (aContainer.id == CustomizableUI.AREA_PANEL) {
      this._showPanelCustomizationPlaceholders();
    }
  },

  onWidgetDestroyed(aWidgetId) {
    let wrapper = this.document.getElementById("wrapper-" + aWidgetId);
    if (wrapper) {
      let wasInPanel = wrapper.parentNode == this.panelUIContents;
      wrapper.remove();
      if (wasInPanel) {
        this._showPanelCustomizationPlaceholders();
      }
    }
  },

  onWidgetAfterCreation(aWidgetId, aArea) {
    // If the node was added to an area, we would have gotten an onWidgetAdded notification,
    // plus associated DOM change notifications, so only do stuff for the palette:
    if (!aArea) {
      let widgetNode = this.document.getElementById(aWidgetId);
      if (widgetNode) {
        this.wrapToolbarItem(widgetNode, "palette");
      } else {
        let widget = CustomizableUI.getWidget(aWidgetId);
        this.visiblePalette.appendChild(this.makePaletteItem(widget, "palette"));
      }
    }
  },

  onAreaNodeRegistered(aArea, aContainer) {
    if (aContainer.ownerDocument == this.document) {
      this._wrapItemsInArea(aContainer);
      this._addDragHandlers(aContainer);
      DragPositionManager.add(this.window, aArea, aContainer);
      this.areas.add(aContainer);
    }
  },

  onAreaNodeUnregistered(aArea, aContainer, aReason) {
    if (aContainer.ownerDocument == this.document && aReason == CustomizableUI.REASON_AREA_UNREGISTERED) {
      this._unwrapItemsInArea(aContainer);
      this._removeDragHandlers(aContainer);
      DragPositionManager.remove(this.window, aArea, aContainer);
      this.areas.delete(aContainer);
    }
  },

  openAddonsManagerThemes(aEvent) {
    aEvent.target.parentNode.parentNode.hidePopup();
    this.window.BrowserOpenAddonsMgr("addons://list/theme");
  },

  getMoreThemes(aEvent) {
    aEvent.target.parentNode.parentNode.hidePopup();
    let getMoreURL = Services.urlFormatter.formatURLPref("lightweightThemes.getMoreURL");
    this.window.openUILinkIn(getMoreURL, "tab");
  },

  updateUIDensity(mode) {
    this.window.gUIDensity.update(mode);
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
  },

  resetUIDensity() {
    this.window.gUIDensity.update();
  },

  onUIDensityMenuShowing() {
    let win = this.window;
    let doc = win.document;
    let gUIDensity = win.gUIDensity;
    let currentDensity = gUIDensity.getCurrentDensity();

    let normalItem = doc.getElementById("customization-uidensity-menuitem-normal");
    normalItem.mode = gUIDensity.MODE_NORMAL;

    let compactItem = doc.getElementById("customization-uidensity-menuitem-compact");
    compactItem.mode = gUIDensity.MODE_COMPACT;

    let items = [normalItem, compactItem];

    let touchItem = doc.getElementById("customization-uidensity-menuitem-touch");
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
      let checkbox = doc.getElementById("customization-uidensity-autotouchmode-checkbox");
      spacer.removeAttribute("hidden");
      checkbox.removeAttribute("hidden");

      // Show a hint that the UI density was overridden automatically.
      if (currentDensity.overridden) {
        let sb = Services.strings.createBundle("chrome://browser/locale/uiDensity.properties");
        touchItem.setAttribute("acceltext",
                                 sb.GetStringFromName("uiDensity.menuitem-touch.acceltext"));
      } else {
        touchItem.removeAttribute("acceltext");
      }

      let autoTouchMode = Services.prefs.getBoolPref(win.gUIDensity.autoTouchModePref);
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

  onLWThemesMenuShowing(aEvent) {
    const DEFAULT_THEME_ID = "{972ce4c6-7e08-4474-a285-3208198ce6fd}";
    const RECENT_LWT_COUNT = 5;

    this._clearLWThemesMenu(aEvent.target);

    function previewTheme(aPreviewThemeEvent) {
      LightweightThemeManager.previewTheme(
        aPreviewThemeEvent.target.theme.id != DEFAULT_THEME_ID ?
        aPreviewThemeEvent.target.theme : null);
    }

    function resetPreview() {
      LightweightThemeManager.resetPreview();
    }

    let onThemeSelected = panel => {
      this._updateLWThemeButtonIcon();
      this._onUIChange();
      panel.hidePopup();
    };

    AddonManager.getAddonByID(DEFAULT_THEME_ID, aDefaultTheme => {
      let doc = this.window.document;

      function buildToolbarButton(aTheme) {
        let tbb = doc.createElement("toolbarbutton");
        tbb.theme = aTheme;
        tbb.setAttribute("label", aTheme.name);
        if (aDefaultTheme == aTheme) {
          // The actual icon is set up so it looks nice in about:addons, but
          // we'd like the version that's correct for the OS we're on, so we set
          // an attribute that our styling will then use to display the icon.
          tbb.setAttribute("defaulttheme", "true");
        } else {
          tbb.setAttribute("image", aTheme.iconURL);
        }
        if (aTheme.description)
          tbb.setAttribute("tooltiptext", aTheme.description);
        tbb.setAttribute("tabindex", "0");
        tbb.classList.add("customization-lwtheme-menu-theme");
        tbb.setAttribute("aria-checked", aTheme.isActive);
        tbb.setAttribute("role", "menuitemradio");
        if (aTheme.isActive) {
          tbb.setAttribute("active", "true");
        }
        tbb.addEventListener("focus", previewTheme);
        tbb.addEventListener("mouseover", previewTheme);
        tbb.addEventListener("blur", resetPreview);
        tbb.addEventListener("mouseout", resetPreview);

        return tbb;
      }

      let themes = [aDefaultTheme];
      let lwts = LightweightThemeManager.usedThemes;
      let currentLwt = LightweightThemeManager.currentTheme;
      let currentLwtIndex = lwts.indexOf(currentLwt);
      if (currentLwtIndex > -1) {
        // Make sure that the current lightweight theme
        // is at the beginning of the array to avoid truncation
        // in the next step.
        lwts = lwts.splice(currentLwtIndex, 1).concat(lwts);
      }
      if (lwts.length > RECENT_LWT_COUNT)
        lwts.length = RECENT_LWT_COUNT;
      for (let lwt of lwts) {
        lwt.isActive = !!currentLwt && (lwt.id == currentLwt.id);
        themes.push(lwt);
      }

      let footer = doc.getElementById("customization-lwtheme-menu-footer");
      let panel = footer.parentNode;
      let recommendedLabel = doc.getElementById("customization-lwtheme-menu-recommended");
      for (let theme of themes) {
        let button = buildToolbarButton(theme);
        button.addEventListener("command", () => {
          if ("userDisabled" in button.theme)
            button.theme.userDisabled = false;
          else
            LightweightThemeManager.currentTheme = button.theme;
          onThemeSelected(panel);
        });
        panel.insertBefore(button, recommendedLabel);
      }

      let lwthemePrefs = Services.prefs.getBranch("lightweightThemes.");
      let recommendedThemes = lwthemePrefs.getStringPref("recommendedThemes");
      recommendedThemes = JSON.parse(recommendedThemes);
      let sb = Services.strings.createBundle("chrome://browser/locale/lightweightThemes.properties");
      for (let theme of recommendedThemes) {
        theme.name = sb.GetStringFromName("lightweightThemes." + theme.id + ".name");
        theme.description = sb.GetStringFromName("lightweightThemes." + theme.id + ".description");
        let button = buildToolbarButton(theme);
        button.addEventListener("command", () => {
          LightweightThemeManager.setLocalTheme(button.theme);
          recommendedThemes = recommendedThemes.filter((aTheme) => { return aTheme.id != button.theme.id; });
          lwthemePrefs.setStringPref("recommendedThemes",
                                     JSON.stringify(recommendedThemes));
          onThemeSelected(panel);
        });
        panel.insertBefore(button, footer);
      }
      let hideRecommendedLabel = (footer.previousSibling == recommendedLabel);
      recommendedLabel.hidden = hideRecommendedLabel;
    });
  },

  _clearLWThemesMenu(panel) {
    let footer = this.document.getElementById("customization-lwtheme-menu-footer");
    let recommendedLabel = this.document.getElementById("customization-lwtheme-menu-recommended");
    for (let element of [footer, recommendedLabel]) {
      while (element.previousSibling &&
             element.previousSibling.localName == "toolbarbutton") {
        element.previousSibling.remove();
      }
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
    let paletteItems = this.visiblePalette.getElementsByTagName("toolbarpaletteitem");
    this.paletteEmptyNotice.hidden = !!paletteItems.length;
  },

  _updateResetButton() {
    let btn = this.document.getElementById("customization-reset-button");
    btn.disabled = CustomizableUI.inDefaultState;
  },

  _updateUndoResetButton() {
    let undoResetButton =  this.document.getElementById("customization-undo-reset-button");
    undoResetButton.hidden = !CustomizableUI.canUndoReset;
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
      case "command":
        if (aEvent.originalTarget == this.window.PanelUI.menuButton) {
          this.exit();
          aEvent.preventDefault();
        }
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

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "nsPref:changed":
        this._updateResetButton();
        this._updateUndoResetButton();
        if (AppConstants.CAN_DRAW_IN_TITLEBAR) {
          this._updateTitlebarCheckbox();
        }
        break;
      case "lightweight-theme-window-updated":
        if (aSubject == this.window) {
          aData = JSON.parse(aData);
          this.updateLWTStyling(aData);
        }
        break;
    }
  },

  _updateTitlebarCheckbox() {
    if (!AppConstants.CAN_DRAW_IN_TITLEBAR) {
      return;
    }
    let drawInTitlebar = Services.prefs.getBoolPref(kDrawInTitlebarPref, true);
    let checkbox = this.document.getElementById("customization-titlebar-visibility-checkbox");
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
    if (!AppConstants.CAN_DRAW_IN_TITLEBAR) {
      return;
    }
    // Drawing in the titlebar means not showing the titlebar, hence the negation:
    Services.prefs.setBoolPref(kDrawInTitlebarPref, !aShouldShowTitlebar);
  },

  get _dwu() {
    if (!this.__dwu) {
      this.__dwu = this.window.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    }
    return this.__dwu;
  },

  get _dir() {
    if (!this.__dir) {
      this.__dir = this.window.getComputedStyle(this.document.documentElement).direction;
    }
    return this.__dir;
  },

  _onDragStart(aEvent) {
    __dumpDragData(aEvent);
    let item = aEvent.target;
    while (item && item.localName != "toolbarpaletteitem") {
      if (item.localName == "toolbar") {
        return;
      }
      item = item.parentNode;
    }

    let draggedItem = item.firstChild;
    let placeForItem = CustomizableUI.getPlaceForItem(item);
    let isRemovable = placeForItem == "palette" ||
                      CustomizableUI.isWidgetRemovable(draggedItem);
    if (item.classList.contains(kPlaceholderClass) || !isRemovable) {
      return;
    }

    let dt = aEvent.dataTransfer;
    let documentId = aEvent.target.ownerDocument.documentElement.id;

    dt.mozSetDataAt(kDragDataTypePrefix + documentId, draggedItem.id, 0);
    dt.effectAllowed = "move";

    let itemRect = this._dwu.getBoundsWithoutFlushing(draggedItem);
    let itemCenter = {x: itemRect.left + itemRect.width / 2,
                      y: itemRect.top + itemRect.height / 2};
    this._dragOffset = {x: aEvent.clientX - itemCenter.x,
                        y: aEvent.clientY - itemCenter.y};

    gDraggingInToolbars = new Set();

    // Hack needed so that the dragimage will still show the
    // item as it appeared before it was hidden.
    this._initializeDragAfterMove = () => {
      // For automated tests, we sometimes start exiting customization mode
      // before this fires, which leaves us with placeholders inserted after
      // we've exited. So we need to check that we are indeed customizing.
      if (this._customizing && !this._transitioning) {
        item.hidden = true;
        this._showPanelCustomizationPlaceholders();
        DragPositionManager.start(this.window);
        let canUsePrevSibling = placeForItem == "toolbar" ||
                                (placeForItem == "panel" && gPhotonStructure);
        if (item.nextSibling) {
          this._setDragActive(item.nextSibling, "before", draggedItem.id, placeForItem);
          this._dragOverItem = item.nextSibling;
        } else if (canUsePrevSibling && item.previousSibling) {
          this._setDragActive(item.previousSibling, "after", draggedItem.id, placeForItem);
          this._dragOverItem = item.previousSibling;
        }
      }
      this._initializeDragAfterMove = null;
      this.window.clearTimeout(this._dragInitializeTimeout);
    };
    this._dragInitializeTimeout = this.window.setTimeout(this._initializeDragAfterMove, 0);
  },

  _onDragOver(aEvent) {
    if (this._isUnwantedDragDrop(aEvent)) {
      return;
    }
    if (this._initializeDragAfterMove) {
      this._initializeDragAfterMove();
    }

    __dumpDragData(aEvent);

    let document = aEvent.target.ownerDocument;
    let documentId = document.documentElement.id;
    if (!aEvent.dataTransfer.mozTypesAt(0)) {
      return;
    }

    let draggedItemId =
      aEvent.dataTransfer.mozGetDataAt(kDragDataTypePrefix + documentId, 0);
    let draggedWrapper = document.getElementById("wrapper-" + draggedItemId);
    let targetArea = this._getCustomizableParent(aEvent.currentTarget);
    let originArea = this._getCustomizableParent(draggedWrapper);

    // Do nothing if the target or origin are not customizable.
    if (!targetArea || !originArea) {
      return;
    }

    // Do nothing if the widget is not allowed to be removed.
    if (targetArea.id == kPaletteId &&
       !CustomizableUI.isWidgetRemovable(draggedItemId)) {
      return;
    }

    // Do nothing if the widget is not allowed to move to the target area.
    if (targetArea.id != kPaletteId &&
        !CustomizableUI.canWidgetMoveToArea(draggedItemId, targetArea.id)) {
      return;
    }

    let targetAreaType = CustomizableUI.getAreaType(targetArea.id);
    let targetNode = this._getDragOverNode(aEvent, targetArea, targetAreaType, draggedItemId);

    // We need to determine the place that the widget is being dropped in
    // the target.
    let dragOverItem, dragValue;
    if (targetNode == targetArea.customizationTarget) {
      // We'll assume if the user is dragging directly over the target, that
      // they're attempting to append a child to that target.
      dragOverItem = (targetAreaType == "toolbar"
                        ? this._findVisiblePreviousSiblingNode(targetNode.lastChild)
                        : targetNode.lastChild) ||
                     targetNode;
      dragValue = "after";
    } else {
      let targetParent = targetNode.parentNode;
      let position = Array.indexOf(targetParent.children, targetNode);
      if (position == -1) {
        dragOverItem = (targetAreaType == "toolbar"
                          ? this._findVisiblePreviousSiblingNode(targetNode.lastChild)
                          : targetNode.lastChild);
        dragValue = "after";
      } else {
        dragOverItem = targetParent.children[position];
        if (targetAreaType == "toolbar") {
          // Check if the aDraggedItem is hovered past the first half of dragOverItem
          let itemRect = this._dwu.getBoundsWithoutFlushing(dragOverItem);
          let dropTargetCenter = itemRect.left + (itemRect.width / 2);
          let existingDir = dragOverItem.getAttribute("dragover");
          if (existingDir == "before") {
            dropTargetCenter += (parseInt(dragOverItem.style.borderInlineStartWidth) || 0) / 2;
          } else {
            dropTargetCenter -= (parseInt(dragOverItem.style.borderInlineEndWidth) || 0) / 2;
          }
          let before = this._dir == "ltr" ? aEvent.clientX < dropTargetCenter : aEvent.clientX > dropTargetCenter;
          dragValue = before ? "before" : "after";
        } else if (targetAreaType == "menu-panel" && gPhotonStructure) {
          let itemRect = this._dwu.getBoundsWithoutFlushing(dragOverItem);
          let dropTargetCenter = itemRect.top + (itemRect.height / 2);
          let existingDir = dragOverItem.getAttribute("dragover");
          if (existingDir == "before") {
            dropTargetCenter += (parseInt(dragOverItem.style.borderBlockStartWidth) || 0) / 2;
          } else {
            dropTargetCenter -= (parseInt(dragOverItem.style.borderBlockEndWidth) || 0) / 2;
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

    if (dragOverItem != this._dragOverItem || dragValue != dragOverItem.getAttribute("dragover")) {
      if (dragOverItem != targetArea.customizationTarget) {
        this._setDragActive(dragOverItem, dragValue, draggedItemId, targetAreaType);
      } else if (targetAreaType == "toolbar") {
        this._updateToolbarCustomizationOutline(this.window, targetArea);
      }
      this._dragOverItem = dragOverItem;
    }

    aEvent.preventDefault();
    aEvent.stopPropagation();
  },

  _onDragDrop(aEvent) {
    if (this._isUnwantedDragDrop(aEvent)) {
      return;
    }

    __dumpDragData(aEvent);
    this._initializeDragAfterMove = null;
    this.window.clearTimeout(this._dragInitializeTimeout);

    let targetArea = this._getCustomizableParent(aEvent.currentTarget);
    let document = aEvent.target.ownerDocument;
    let documentId = document.documentElement.id;
    let draggedItemId =
      aEvent.dataTransfer.mozGetDataAt(kDragDataTypePrefix + documentId, 0);
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
      if (targetNode.nextSibling) {
        targetNode = targetNode.nextSibling;
      } else {
        targetNode = targetArea;
      }
    }
    // If the target node is a placeholder, get its sibling as the real target.
    while (targetNode.classList.contains(kPlaceholderClass) && targetNode.nextSibling) {
      targetNode = targetNode.nextSibling;
    }
    if (targetNode.tagName == "toolbarpaletteitem") {
      targetNode = targetNode.firstChild;
    }

    this._cancelDragActive(this._dragOverItem, null, true);
    this._removePanelCustomizationPlaceholders();

    try {
      this._applyDrop(aEvent, targetArea, originArea, draggedItemId, targetNode);
    } catch (ex) {
      log.error(ex, ex.stack);
    }

    this._showPanelCustomizationPlaceholders();
  },

  _applyDrop(aEvent, aTargetArea, aOriginArea, aDraggedItemId, aTargetNode) {
    let document = aEvent.target.ownerDocument;
    let draggedItem = document.getElementById(aDraggedItemId);
    draggedItem.hidden = false;
    draggedItem.removeAttribute("mousedown");

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

        CustomizableUI.removeWidgetFromArea(aDraggedItemId);
        BrowserUITelemetry.countCustomizationEvent("remove");
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
      if (aOriginArea.id !== kPaletteId) {
        // The dragend event already fires when the item moves within the palette.
        this._onDragEnd(aEvent);
      }
      return;
    }

    if (!CustomizableUI.canWidgetMoveToArea(aDraggedItemId, aTargetArea.id)) {
      return;
    }

    // Skipintoolbarset items won't really be moved:
    if (draggedItem.getAttribute("skipintoolbarset") == "true") {
      // These items should never leave their area:
      if (aTargetArea != aOriginArea) {
        return;
      }
      let place = draggedItem.parentNode.getAttribute("place");
      this.unwrapToolbarItem(draggedItem.parentNode);
      if (aTargetNode == aTargetArea.customizationTarget) {
        aTargetArea.customizationTarget.appendChild(draggedItem);
      } else {
        this.unwrapToolbarItem(aTargetNode.parentNode);
        aTargetArea.customizationTarget.insertBefore(draggedItem, aTargetNode);
        this.wrapToolbarItem(aTargetNode, place);
      }
      this.wrapToolbarItem(draggedItem, place);
      BrowserUITelemetry.countCustomizationEvent("move");
      return;
    }

    // Is the target the customization area itself? If so, we just add the
    // widget to the end of the area.
    if (aTargetNode == aTargetArea.customizationTarget) {
      CustomizableUI.addWidgetToArea(aDraggedItemId, aTargetArea.id);
      // For the purposes of BrowserUITelemetry, we consider both moving a widget
      // within the same area, and adding a widget from one area to another area
      // as a "move". An "add" is only when we move an item from the palette into
      // an area.
      let custEventType = aOriginArea.id == kPaletteId ? "add" : "move";
      BrowserUITelemetry.countCustomizationEvent(custEventType);
      this._onDragEnd(aEvent);
      return;
    }

    // We need to determine the place that the widget is being dropped in
    // the target.
    let placement;
    let itemForPlacement = aTargetNode;
    // Skip the skipintoolbarset items when determining the place of the item:
    while (itemForPlacement && itemForPlacement.getAttribute("skipintoolbarset") == "true" &&
           itemForPlacement.parentNode &&
           itemForPlacement.parentNode.nodeName == "toolbarpaletteitem") {
      itemForPlacement = itemForPlacement.parentNode.nextSibling;
      if (itemForPlacement && itemForPlacement.nodeName == "toolbarpaletteitem") {
        itemForPlacement = itemForPlacement.firstChild;
      }
    }
    if (itemForPlacement && !itemForPlacement.classList.contains(kPlaceholderClass)) {
      let targetNodeId = (itemForPlacement.nodeName == "toolbarpaletteitem") ?
                            itemForPlacement.firstChild && itemForPlacement.firstChild.id :
                            itemForPlacement.id;
      placement = CustomizableUI.getPlacementOfWidget(targetNodeId);
    }
    if (!placement) {
      log.debug("Could not get a position for " + aTargetNode.nodeName + "#" + aTargetNode.id + "." + aTargetNode.className);
    }
    let position = placement ? placement.position : null;

    // Force creating a new spacer/spring/separator if dragging from the palette
    if (CustomizableUI.isSpecialWidget(aDraggedItemId) && aOriginArea.id == kPaletteId) {
      aDraggedItemId = aDraggedItemId.match(/^customizableui-special-(spring|spacer|separator)/)[1];
    }

    // Is the target area the same as the origin? Since we've already handled
    // the possibility that the target is the customization palette, we know
    // that the widget is moving within a customizable area.
    if (aTargetArea == aOriginArea) {
      CustomizableUI.moveWidgetWithinArea(aDraggedItemId, position);
    } else {
      CustomizableUI.addWidgetToArea(aDraggedItemId, aTargetArea.id, position);
    }

    this._onDragEnd(aEvent);

    // For BrowserUITelemetry, an "add" is only when we move an item from the palette
    // into an area. Otherwise, it's a move.
    let custEventType = aOriginArea.id == kPaletteId ? "add" : "move";
    BrowserUITelemetry.countCustomizationEvent(custEventType);

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

    let draggedItemId =
      aEvent.dataTransfer.mozGetDataAt(kDragDataTypePrefix + documentId, 0);

    let draggedWrapper = document.getElementById("wrapper-" + draggedItemId);

    // DraggedWrapper might no longer available if a widget node is
    // destroyed after starting (but before stopping) a drag.
    if (draggedWrapper) {
      draggedWrapper.hidden = false;
      draggedWrapper.removeAttribute("mousedown");
    }

    if (this._dragOverItem) {
      this._cancelDragActive(this._dragOverItem);
      this._dragOverItem = null;
    }
    this._updateToolbarCustomizationOutline(this.window);
    this._showPanelCustomizationPlaceholders();
    DragPositionManager.stop();
  },

  _isUnwantedDragDrop(aEvent) {
    // The simulated events generated by synthesizeDragStart/synthesizeDrop in
    // mochitests are used only for testing whether the right data is being put
    // into the dataTransfer. Neither cause a real drop to occur, so they don't
    // set the source node. There isn't a means of testing real drag and drops,
    // so this pref skips the check but it should only be set by test code.
    if (this._skipSourceNodeCheck) {
      return false;
    }

    /* Discard drag events that originated from a separate window to
       prevent content->chrome privilege escalations. */
    let mozSourceNode = aEvent.dataTransfer.mozSourceNode;
    // mozSourceNode is null in the dragStart event handler or if
    // the drag event originated in an external application.
    return !mozSourceNode ||
           mozSourceNode.ownerGlobal != this.window;
  },

  _setDragActive(aItem, aValue, aDraggedItemId, aAreaType) {
    if (!aItem) {
      return;
    }

    // getPlaceForItem and getAreaType return different things. Hack-around
    // rather than having to update every. single. consumer. (and break add-ons)
    if (aAreaType == "panel") {
      aAreaType = "menu-panel";
    }
    if (aItem.getAttribute("dragover") != aValue) {
      aItem.setAttribute("dragover", aValue);

      let window = aItem.ownerGlobal;
      let draggedItem = window.document.getElementById(aDraggedItemId);
      if (aAreaType == "palette" || (aAreaType == "menu-panel" && !gPhotonStructure)) {
        this._setGridDragActive(aItem, draggedItem, aValue);
      } else {
        let targetArea = this._getCustomizableParent(aItem);
        this._updateToolbarCustomizationOutline(window, targetArea);
        let makeSpaceImmediately = false;
        if (!gDraggingInToolbars.has(targetArea.id)) {
          gDraggingInToolbars.add(targetArea.id);
          let draggedWrapper = this.document.getElementById("wrapper-" + aDraggedItemId);
          let originArea = this._getCustomizableParent(draggedWrapper);
          makeSpaceImmediately = originArea == targetArea;
        }
        let propertyToMeasure = aAreaType == "toolbar" ? "width" : "height";
        // Calculate width/height of the item when it'd be dropped in this position.
        let borderWidth = this._getDragItemSize(aItem, draggedItem)[propertyToMeasure];
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
    this._updateToolbarCustomizationOutline(aItem.ownerGlobal);
    let currentArea = this._getCustomizableParent(aItem);
    if (!currentArea) {
      return;
    }
    let areaType = CustomizableUI.getAreaType(currentArea.id);
    let needPositionManager = !areaType || (areaType == "menu-panel" && !gPhotonStructure);
    if (!needPositionManager) {
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
        let nextArea = this._getCustomizableParent(aNextItem);
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
    let draggedWrapper = this.document.getElementById("wrapper-" + aDraggedItem.id);
    let originArea = this._getCustomizableParent(draggedWrapper);
    let positionManager = DragPositionManager.getManagerForArea(targetArea);
    let draggedSize = this._getDragItemSize(aDragOverNode, aDraggedItem);
    let isWide = aDraggedItem.classList.contains(CustomizableUI.WIDE_PANEL_CLASS);
    positionManager.insertPlaceholder(targetArea, aDragOverNode, isWide, draggedSize,
                                      originArea == targetArea);
  },

  _getDragItemSize(aDragOverNode, aDraggedItem) {
    // Cache it good, cache it real good.
    if (!this._dragSizeMap)
      this._dragSizeMap = new WeakMap();
    if (!this._dragSizeMap.has(aDraggedItem))
      this._dragSizeMap.set(aDraggedItem, new WeakMap());
    let itemMap = this._dragSizeMap.get(aDraggedItem);
    let targetArea = this._getCustomizableParent(aDragOverNode);
    let currentArea = this._getCustomizableParent(aDraggedItem);
    // Return the size for this target from cache, if it exists.
    let size = itemMap.get(targetArea);
    if (size)
      return size;

    // Calculate size of the item when it'd be dropped in this position.
    let currentParent = aDraggedItem.parentNode;
    let currentSibling = aDraggedItem.nextSibling;
    const kAreaType = "cui-areatype";
    let areaType, currentType;

    if (targetArea != currentArea) {
      // Move the widget temporarily next to the placeholder.
      aDragOverNode.parentNode.insertBefore(aDraggedItem, aDragOverNode);
      // Update the node's areaType.
      areaType = CustomizableUI.getAreaType(targetArea.id);
      currentType = aDraggedItem.hasAttribute(kAreaType) &&
                    aDraggedItem.getAttribute(kAreaType);
      if (areaType)
        aDraggedItem.setAttribute(kAreaType, areaType);
      this.wrapToolbarItem(aDraggedItem, areaType || "palette");
      CustomizableUI.onWidgetDrag(aDraggedItem.id, targetArea.id);
    } else {
      aDraggedItem.parentNode.hidden = false;
    }

    // Fetch the new size.
    let rect = aDraggedItem.parentNode.getBoundingClientRect();
    size = {width: rect.width, height: rect.height};
    // Cache the found value of size for this target.
    itemMap.set(targetArea, size);

    if (targetArea != currentArea) {
      this.unwrapToolbarItem(aDraggedItem.parentNode);
      // Put the item back into its previous position.
      currentParent.insertBefore(aDraggedItem, currentSibling);
      // restore the areaType
      if (areaType) {
        if (currentType === false)
          aDraggedItem.removeAttribute(kAreaType);
        else
          aDraggedItem.setAttribute(kAreaType, currentType);
      }
      this.createOrUpdateWrapper(aDraggedItem, null, true);
      CustomizableUI.onWidgetDrag(aDraggedItem.id);
    } else {
      aDraggedItem.parentNode.hidden = true;
    }
    return size;
  },

  _getCustomizableParent(aElement) {
    if (gPhotonStructure && aElement) {
      // Deal with drag/drop on the padding of the panel in photon.
      let containingPanelHolder = aElement.closest("#customization-panelHolder");
      if (containingPanelHolder) {
        return containingPanelHolder.firstChild;
      }
    }

    let areas = CustomizableUI.areas;
    areas.push(kPaletteId);
    while (aElement) {
      if (areas.indexOf(aElement.id) != -1) {
        return aElement;
      }
      aElement = aElement.parentNode;
    }

    return null;
  },

  _getDragOverNode(aEvent, aAreaElement, aAreaType, aDraggedItemId) {
    let expectedParent = aAreaElement.customizationTarget || aAreaElement;
    // Our tests are stupid. Cope:
    if (!aEvent.clientX && !aEvent.clientY) {
      return aEvent.target;
    }
    // Offset the drag event's position with the offset to the center of
    // the thing we're dragging
    let dragX = aEvent.clientX - this._dragOffset.x;
    let dragY = aEvent.clientY - this._dragOffset.y;

    // Ensure this is within the container
    let boundsContainer = expectedParent;
    // NB: because the panel UI itself is inside a scrolling container, we need
    // to use the parent bounds (otherwise, if the panel UI is scrolled down,
    // the numbers we get are in window coordinates which leads to various kinds
    // of weirdness)
    if (boundsContainer == this.panelUIContents) {
      boundsContainer = boundsContainer.parentNode;
    }
    let bounds = boundsContainer.getBoundingClientRect();
    dragX = Math.min(bounds.right, Math.max(dragX, bounds.left));
    dragY = Math.min(bounds.bottom, Math.max(dragY, bounds.top));

    let targetNode;
    if (aAreaType == "toolbar" || (aAreaType == "menu-panel" && gPhotonStructure)) {
      targetNode = aAreaElement.ownerDocument.elementFromPoint(dragX, dragY);
      while (targetNode && targetNode.parentNode != expectedParent) {
        targetNode = targetNode.parentNode;
      }
    } else {
      let positionManager = DragPositionManager.getManagerForArea(aAreaElement);
      // Make it relative to the container:
      dragX -= bounds.left;
      // NB: but if we're in the panel UI, we need to use the actual panel
      // contents instead of the scrolling container to determine our origin
      // offset against:
      if (expectedParent == this.panelUIContents) {
        dragY -= this.panelUIContents.getBoundingClientRect().top;
      } else {
        dragY -= bounds.top;
      }
      // Find the closest node:
      targetNode = positionManager.find(aAreaElement, dragX, dragY, aDraggedItemId);
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
    if (item && !item.classList.contains(kPlaceholderClass) &&
        item.getAttribute("removable") == "true") {
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
      if (aElement.localName == "toolbar")
        return null;
      aElement = aElement.parentNode;
    }
    return aElement;
  },

  _showPanelCustomizationPlaceholders() {
    let doc = this.document;
    let contents = this.panelUIContents;
    let narrowItemsAfterWideItem = 0;
    let node = contents.lastChild;
    while (node && !node.classList.contains(CustomizableUI.WIDE_PANEL_CLASS) &&
           (!node.firstChild || !node.firstChild.classList.contains(CustomizableUI.WIDE_PANEL_CLASS))) {
      if (!node.hidden && !node.classList.contains(kPlaceholderClass)) {
        narrowItemsAfterWideItem++;
      }
      node = node.previousSibling;
    }

    let orphanedItems = narrowItemsAfterWideItem % CustomizableUI.PANEL_COLUMN_COUNT;
    let placeholders = CustomizableUI.PANEL_COLUMN_COUNT - orphanedItems;

    let currentPlaceholderCount = contents.querySelectorAll("." + kPlaceholderClass).length;
    if (placeholders > currentPlaceholderCount) {
      while (placeholders-- > currentPlaceholderCount) {
        let placeholder = doc.createElement("toolbarpaletteitem");
        placeholder.classList.add(kPlaceholderClass);
        // XXXjaws The toolbarbutton child here is only necessary to get
        //  the styling right here.
        let placeholderChild = doc.createElement("toolbarbutton");
        placeholderChild.classList.add(kPlaceholderClass + "-child");
        placeholder.appendChild(placeholderChild);
        contents.appendChild(placeholder);
      }
    } else if (placeholders < currentPlaceholderCount) {
      while (placeholders++ < currentPlaceholderCount) {
        contents.querySelectorAll("." + kPlaceholderClass)[0].remove();
      }
    }
  },

  _removePanelCustomizationPlaceholders() {
    let contents = this.panelUIContents;
    let oldPlaceholders = contents.getElementsByClassName(kPlaceholderClass);
    while (oldPlaceholders.length) {
      contents.removeChild(oldPlaceholders[0]);
    }
  },

  /**
   * Update toolbar customization targets during drag events to add or remove
   * outlines to indicate that an area is customizable.
   *
   * @param aWindow                       The XUL window in which outlines should be updated.
   * @param {Element} [aToolbarArea=null] The element of the customizable toolbar area to add the
   *                                      outline to. If aToolbarArea is falsy, the outline will be
   *                                      removed from all toolbar areas.
   */
  _updateToolbarCustomizationOutline(aWindow, aToolbarArea = null) {
    // Remove the attribute from existing customization targets
    for (let area of CustomizableUI.areas) {
      if (CustomizableUI.getAreaType(area) != CustomizableUI.TYPE_TOOLBAR) {
        continue;
      }
      let target = CustomizableUI.getCustomizeTargetForArea(area, aWindow);
      target.removeAttribute("customizing-dragovertarget");
    }

    // Now set the attribute on the desired target
    if (aToolbarArea) {
      if (CustomizableUI.getAreaType(aToolbarArea.id) != CustomizableUI.TYPE_TOOLBAR)
        return;
      let target = CustomizableUI.getCustomizeTargetForArea(aToolbarArea.id, aWindow);
      target.setAttribute("customizing-dragovertarget", true);
    }
  },

  _findVisiblePreviousSiblingNode(aReferenceNode) {
    while (aReferenceNode &&
           aReferenceNode.localName == "toolbarpaletteitem" &&
           aReferenceNode.firstChild.hidden) {
      aReferenceNode = aReferenceNode.previousSibling;
    }
    return aReferenceNode;
  },

  onPanelContextMenuShowing(event) {
    let inPermanentArea = !gPhotonStructure ||
                          !!event.target.triggerNode.closest("#widget-overflow-fixed-list");
    let doc = event.target.ownerDocument;
    doc.getElementById("customizationPanelItemContextMenuUnpin").hidden = !inPermanentArea;
    doc.getElementById("customizationPanelItemContextMenuPin").hidden = inPermanentArea;
  },
};

function __dumpDragData(aEvent, caller) {
  if (!gDebug) {
    return;
  }
  let str = "Dumping drag data (" + (caller ? caller + " in " : "") + "CustomizeMode.jsm) {\n";
  str += "  type: " + aEvent["type"] + "\n";
  for (let el of ["target", "currentTarget", "relatedTarget"]) {
    if (aEvent[el]) {
      str += "  " + el + ": " + aEvent[el] + "(localName=" + aEvent[el].localName + "; id=" + aEvent[el].id + ")\n";
    }
  }
  for (let prop in aEvent.dataTransfer) {
    if (typeof aEvent.dataTransfer[prop] != "function") {
      str += "  dataTransfer[" + prop + "]: " + aEvent.dataTransfer[prop] + "\n";
    }
  }
  str += "}";
  log.debug(str);
}

function dispatchFunction(aFunc) {
  Services.tm.dispatchToMainThread(aFunc);
}
