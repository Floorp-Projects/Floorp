/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Note: the file browser-tabsintitlebar-stub.js is used instead of
// this one on platforms which don't have CAN_DRAW_IN_TITLEBAR defined.

var TabsInTitlebar = {
  init() {
    if (this._initialized) {
      return;
    }
    this._readPref();
    Services.prefs.addObserver(this._prefName, this);

    // We need to update the appearance of the titlebar when the menu changes
    // from the active to the inactive state. We can't, however, rely on
    // DOMMenuBarInactive, because the menu fires this event and then removes
    // the inactive attribute after an event-loop spin.
    //
    // Because updating the appearance involves sampling the heights and margins
    // of various elements, it's important that the layout be more or less
    // settled before updating the titlebar. So instead of listening to
    // DOMMenuBarActive and DOMMenuBarInactive, we use a MutationObserver to
    // watch the "invalid" attribute directly.
    let menu = document.getElementById("toolbar-menubar");
    this._menuObserver = new MutationObserver(this._onMenuMutate);
    this._menuObserver.observe(menu, {attributes: true});

    this.onAreaReset = function(aArea) {
      if (aArea == CustomizableUI.AREA_TABSTRIP || aArea == CustomizableUI.AREA_MENUBAR)
        this._update(true);
    };
    this.onWidgetAdded = this.onWidgetRemoved = function(aWidgetId, aArea) {
      if (aArea == CustomizableUI.AREA_TABSTRIP || aArea == CustomizableUI.AREA_MENUBAR)
        this._update(true);
    };
    CustomizableUI.addListener(this);

    addEventListener("resolutionchange", this, false);

    this._initialized = true;
    if (this._updateOnInit) {
      // We don't need to call this with 'true', even if original calls
      // (before init()) did, because this will be the first call and so
      // we will update anyway.
      this._update();
    }
  },

  allowedBy(condition, allow) {
    if (allow) {
      if (condition in this._disallowed) {
        delete this._disallowed[condition];
        this._update(true);
      }
    } else if (!(condition in this._disallowed)) {
      this._disallowed[condition] = null;
      this._update(true);
    }
  },

  updateAppearance: function updateAppearance(aForce) {
    this._update(aForce);
  },

  get enabled() {
    return document.documentElement.getAttribute("tabsintitlebar") == "true";
  },

  observe(subject, topic, data) {
    if (topic == "nsPref:changed")
      this._readPref();
  },

  handleEvent(aEvent) {
    if (aEvent.type == "resolutionchange" && aEvent.target == window) {
      this._update(true);
    }
  },

  _onMenuMutate(aMutations) {
    for (let mutation of aMutations) {
      if (mutation.attributeName == "inactive" ||
          mutation.attributeName == "autohide") {
        TabsInTitlebar._update(true);
        return;
      }
    }
  },

  _initialized: false,
  _updateOnInit: false,
  _disallowed: {},
  _prefName: "browser.tabs.drawInTitlebar",
  _lastSizeMode: null,

  _readPref() {
    this.allowedBy("pref",
                   Services.prefs.getBoolPref(this._prefName));
  },

  _update(aForce = false) {
    let $ = id => document.getElementById(id);
    let rect = ele => ele.getBoundingClientRect();
    let verticalMargins = cstyle => parseFloat(cstyle.marginBottom) + parseFloat(cstyle.marginTop);

    if (window.fullScreen)
      return;

    // In some edgecases it is possible for this to fire before we've initialized.
    // Don't run now, but don't forget to run it when we do initialize.
    if (!this._initialized) {
      this._updateOnInit = true;
      return;
    }

    if (!aForce) {
      // _update is called on resize events, because the window is not ready
      // after sizemode events. However, we only care about the event when the
      // sizemode is different from the last time we updated the appearance of
      // the tabs in the titlebar.
      let sizemode = document.documentElement.getAttribute("sizemode");
      if (this._lastSizeMode == sizemode) {
        return;
      }
      let oldSizeMode = this._lastSizeMode;
      this._lastSizeMode = sizemode;
      // Don't update right now if we are leaving fullscreen, since the UI is
      // still changing in the consequent "fullscreen" event. Code there will
      // call this function again when everything is ready.
      // See browser-fullScreen.js: FullScreen.toggle and bug 1173768.
      if (oldSizeMode == "fullscreen") {
        return;
      }
    }

    let allowed = (Object.keys(this._disallowed)).length == 0;

    let titlebar = $("titlebar");
    let titlebarContent = $("titlebar-content");
    let menubar = $("toolbar-menubar");

    if (allowed) {
      // We set the tabsintitlebar attribute first so that our CSS for
      // tabsintitlebar manifests before we do our measurements.
      document.documentElement.setAttribute("tabsintitlebar", "true");
      updateTitlebarDisplay();

      // Try to avoid reflows in this code by calculating dimensions first and
      // then later set the properties affecting layout together in a batch.

      // Get the full height of the tabs toolbar:
      let tabsToolbar = $("TabsToolbar");
      let tabsStyles = window.getComputedStyle(tabsToolbar);
      let fullTabsHeight = rect(tabsToolbar).height + verticalMargins(tabsStyles);
      // Buttons first:
      let captionButtonsBoxWidth = rect($("titlebar-buttonbox-container")).width;

      let secondaryButtonsWidth, menuHeight, fullMenuHeight, menuStyles;
      if (AppConstants.platform == "macosx") {
        secondaryButtonsWidth = rect($("titlebar-secondary-buttonbox")).width;
        // No need to look up the menubar stuff on OS X:
        menuHeight = 0;
        fullMenuHeight = 0;
      } else {
        // Otherwise, get the height and margins separately for the menubar
        menuHeight = rect(menubar).height;
        menuStyles = window.getComputedStyle(menubar);
        fullMenuHeight = verticalMargins(menuStyles) + menuHeight;
      }

      // And get the height of what's in the titlebar:
      let titlebarContentHeight = rect(titlebarContent).height;

      // Begin setting CSS properties which will cause a reflow

      if (AppConstants.MOZ_PHOTON_THEME &&
          AppConstants.isPlatformAndVersionAtLeast("win", "10.0")) {
        if (!menuHeight) {
          titlebarContentHeight = Math.max(titlebarContentHeight, fullTabsHeight);
          $("titlebar-buttonbox").style.height = titlebarContentHeight + "px";
        } else {
          $("titlebar-buttonbox").style.removeProperty("height");
        }
      }

      // If the menubar is around (menuHeight is non-zero), try to adjust
      // its full height (i.e. including margins) to match the titlebar,
      // by changing the menubar's bottom padding
      if (menuHeight) {
        // Calculate the difference between the titlebar's height and that of the menubar
        let menuTitlebarDelta = titlebarContentHeight - fullMenuHeight;
        let paddingBottom;
        // The titlebar is bigger:
        if (menuTitlebarDelta > 0) {
          fullMenuHeight += menuTitlebarDelta;
          // If there is already padding on the menubar, we need to add that
          // to the difference so the total padding is correct:
          if ((paddingBottom = menuStyles.paddingBottom)) {
            menuTitlebarDelta += parseFloat(paddingBottom);
          }
          menubar.style.paddingBottom = menuTitlebarDelta + "px";
        // The menubar is bigger, but has bottom padding we can remove:
        } else if (menuTitlebarDelta < 0 && (paddingBottom = menuStyles.paddingBottom)) {
          let existingPadding = parseFloat(paddingBottom);
          // menuTitlebarDelta is negative; work out what's left, but don't set negative padding:
          let desiredPadding = Math.max(0, existingPadding + menuTitlebarDelta);
          menubar.style.paddingBottom = desiredPadding + "px";
          // We've changed the menu height now:
          fullMenuHeight += desiredPadding - existingPadding;
        }
      }

      // Next, we calculate how much we need to stretch the titlebar down to
      // go all the way to the bottom of the tab strip, if necessary.
      let tabAndMenuHeight = fullTabsHeight + fullMenuHeight;

      if (tabAndMenuHeight > titlebarContentHeight) {
        // We need to increase the titlebar content's outer height (ie including margins)
        // to match the tab and menu height:
        let extraMargin = tabAndMenuHeight - titlebarContentHeight;
        if (AppConstants.platform != "macosx") {
          titlebarContent.style.marginBottom = extraMargin + "px";
        }

        titlebarContentHeight += extraMargin;
      } else {
        titlebarContent.style.removeProperty("margin-bottom");
      }

      // Then add a negative margin to the titlebar, so that the following elements
      // will overlap it by the lesser of the titlebar height or the tabstrip+menu.
      let minTitlebarOrTabsHeight = Math.min(titlebarContentHeight, tabAndMenuHeight);
      titlebar.style.marginBottom = "-" + minTitlebarOrTabsHeight + "px";

      // Finally, size the placeholders:
      if (AppConstants.platform == "macosx") {
        this._sizePlaceholder("fullscreen-button", secondaryButtonsWidth);
      }
      this._sizePlaceholder("caption-buttons", captionButtonsBoxWidth);

    } else {
      document.documentElement.removeAttribute("tabsintitlebar");
      updateTitlebarDisplay();

      if (AppConstants.platform == "macosx") {
        let secondaryButtonsWidth = rect($("titlebar-secondary-buttonbox")).width;
        this._sizePlaceholder("fullscreen-button", secondaryButtonsWidth);
      }

      // Reset the margins and padding that might have been modified:
      titlebarContent.style.marginTop = "";
      titlebarContent.style.marginBottom = "";
      titlebar.style.marginBottom = "";
      menubar.style.paddingBottom = "";
    }

    ToolbarIconColor.inferFromText("tabsintitlebar", TabsInTitlebar.enabled);

    if (CustomizationHandler.isCustomizing()) {
      gCustomizeMode.updateLWTStyling();
    }
  },

  _sizePlaceholder(type, width) {
    Array.forEach(document.querySelectorAll(".titlebar-placeholder[type='" + type + "']"),
                  function(node) { node.width = width; });
  },

  uninit() {
    this._initialized = false;
    removeEventListener("resolutionchange", this);
    Services.prefs.removeObserver(this._prefName, this);
    this._menuObserver.disconnect();
    CustomizableUI.removeListener(this);
  }
};

function updateTitlebarDisplay() {
  if (AppConstants.platform == "macosx") {
    // OS X and the other platforms differ enough to necessitate this kind of
    // special-casing. Like the other platforms where we CAN_DRAW_IN_TITLEBAR,
    // we draw in the OS X titlebar when putting the tabs up there. However, OS X
    // also draws in the titlebar when a lightweight theme is applied, regardless
    // of whether or not the tabs are drawn in the titlebar.
    if (TabsInTitlebar.enabled) {
      document.documentElement.setAttribute("chromemargin-nonlwtheme", "0,-1,-1,-1");
      document.documentElement.setAttribute("chromemargin", "0,-1,-1,-1");
      document.documentElement.removeAttribute("drawtitle");
    } else {
      // We set chromemargin-nonlwtheme to "" instead of removing it as a way of
      // making sure that LightweightThemeConsumer doesn't take it upon itself to
      // detect this value again if and when we do a lwtheme state change.
      document.documentElement.setAttribute("chromemargin-nonlwtheme", "");
      let isCustomizing = document.documentElement.hasAttribute("customizing");
      let hasLWTheme = document.documentElement.hasAttribute("lwtheme");
      let isPrivate = PrivateBrowsingUtils.isWindowPrivate(window);
      if ((!hasLWTheme || isCustomizing) && !isPrivate) {
        document.documentElement.removeAttribute("chromemargin");
      }
      document.documentElement.setAttribute("drawtitle", "true");
    }
  } else if (TabsInTitlebar.enabled) {
    // not OS X
    document.documentElement.setAttribute("chromemargin", "0,2,2,2");
  } else {
    document.documentElement.removeAttribute("chromemargin");
  }
}

function onTitlebarMaxClick() {
  if (window.windowState == window.STATE_MAXIMIZED)
    window.restore();
  else
    window.maximize();
}
