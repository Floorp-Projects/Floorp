/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Handles the indicator that displays the progress of ongoing downloads, which
 * is also used as the anchor for the downloads panel.
 *
 * This module includes the following constructors and global objects:
 *
 * DownloadsButton
 * Main entry point for the downloads indicator.  Depending on how the toolbars
 * have been customized, this object determines if we should show a fully
 * functional indicator, a placeholder used during customization and in the
 * customization palette, or a neutral view as a temporary anchor for the
 * downloads panel.
 *
 * DownloadsIndicatorView
 * Builds and updates the actual downloads status widget, responding to changes
 * in the global status data, or provides a neutral view if the indicator is
 * removed from the toolbars and only used as a temporary anchor.  In addition,
 * handles the user interaction events raised by the widget.
 */

"use strict";

////////////////////////////////////////////////////////////////////////////////
//// DownloadsButton

/**
 * Main entry point for the downloads indicator.  Depending on how the toolbars
 * have been customized, this object determines if we should show a fully
 * functional indicator, a placeholder used during customization and in the
 * customization palette, or a neutral view as a temporary anchor for the
 * downloads panel.
 */
const DownloadsButton = {
  /**
   * Location of the indicator overlay.
   */
  get kIndicatorOverlay() {
    return "chrome://browser/content/downloads/indicatorOverlay.xul";
  },

  /**
   * Returns a reference to the downloads button position placeholder, or null
   * if not available because it has been removed from the toolbars.
   */
  get _placeholder() {
    return document.getElementById("downloads-button");
  },

  /**
   * This function is called asynchronously just after window initialization.
   *
   * NOTE: This function should limit the input/output it performs to improve
   *       startup time.
   */
  initializeIndicator() {
    DownloadsIndicatorView.ensureInitialized();
  },

  /**
   * Indicates whether toolbar customization is in progress.
   */
  _customizing: false,

  /**
   * This function is called when toolbar customization starts.
   *
   * During customization, we never show the actual download progress indication
   * or the event notifications, but we show a neutral placeholder.  The neutral
   * placeholder is an ordinary button defined in the browser window that can be
   * moved freely between the toolbars and the customization palette.
   */
  customizeStart() {
    // Prevent the indicator from being displayed as a temporary anchor
    // during customization, even if requested using the getAnchor method.
    this._customizing = true;
    this._anchorRequested = false;
  },

  /**
   * This function is called when toolbar customization ends.
   */
  customizeDone() {
    this._customizing = false;
    DownloadsIndicatorView.afterCustomize();
  },

  /**
   * Determines the position where the indicator should appear, and moves its
   * associated element to the new position.
   *
   * @return Anchor element, or null if the indicator is not visible.
   */
  _getAnchorInternal() {
    let indicator = DownloadsIndicatorView.indicator;
    if (!indicator) {
      // Exit now if the indicator overlay isn't loaded yet, or if the button
      // is not in the document.
      return null;
    }

    indicator.open = this._anchorRequested;

    let widget = CustomizableUI.getWidget("downloads-button")
                               .forWindow(window);
     // Determine if the indicator is located on an invisible toolbar.
     if (!isElementVisible(indicator.parentNode) && !widget.overflowed) {
       return null;
     }

    return DownloadsIndicatorView.indicatorAnchor;
  },

  /**
   * Checks whether the indicator is, or will soon be visible in the browser
   * window.
   *
   * @param aCallback
   *        Called once the indicator overlay has loaded. Gets a boolean
   *        argument representing the indicator visibility.
   */
  checkIsVisible(aCallback) {
    DownloadsOverlayLoader.ensureOverlayLoaded(this.kIndicatorOverlay, () => {
      if (!this._placeholder) {
        aCallback(false);
      } else {
        let element = DownloadsIndicatorView.indicator || this._placeholder;
        aCallback(isElementVisible(element.parentNode));
      }
    });
  },

  /**
   * Indicates whether we should try and show the indicator temporarily as an
   * anchor for the panel, even if the indicator would be hidden by default.
   */
  _anchorRequested: false,

  /**
   * Ensures that there is an anchor available for the panel.
   *
   * @param aCallback
   *        Called when the anchor is available, passing the element where the
   *        panel should be anchored, or null if an anchor is not available (for
   *        example because both the tab bar and the navigation bar are hidden).
   */
  getAnchor(aCallback) {
    // Do not allow anchoring the panel to the element while customizing.
    if (this._customizing) {
      aCallback(null);
      return;
    }

    DownloadsOverlayLoader.ensureOverlayLoaded(this.kIndicatorOverlay, () => {
      this._anchorRequested = true;
      aCallback(this._getAnchorInternal());
    });
  },

  /**
   * Allows the temporary anchor to be hidden.
   */
  releaseAnchor() {
    this._anchorRequested = false;
    this._getAnchorInternal();
  },

  get _tabsToolbar() {
    delete this._tabsToolbar;
    return this._tabsToolbar = document.getElementById("TabsToolbar");
  },

  get _navBar() {
    delete this._navBar;
    return this._navBar = document.getElementById("nav-bar");
  }
};

Object.defineProperty(this, "DownloadsButton", {
  value: DownloadsButton,
  enumerable: true,
  writable: false
});

////////////////////////////////////////////////////////////////////////////////
//// DownloadsIndicatorView

/**
 * Builds and updates the actual downloads status widget, responding to changes
 * in the global status data, or provides a neutral view if the indicator is
 * removed from the toolbars and only used as a temporary anchor.  In addition,
 * handles the user interaction events raised by the widget.
 */
const DownloadsIndicatorView = {
  /**
   * True when the view is connected with the underlying downloads data.
   */
  _initialized: false,

  /**
   * True when the user interface elements required to display the indicator
   * have finished loading in the browser window, and can be referenced.
   */
  _operational: false,

  /**
   * Prepares the downloads indicator to be displayed.
   */
  ensureInitialized() {
    if (this._initialized) {
      return;
    }
    this._initialized = true;

    window.addEventListener("unload", this.onWindowUnload, false);
    DownloadsCommon.getIndicatorData(window).addView(this);
  },

  /**
   * Frees the internal resources related to the indicator.
   */
  ensureTerminated() {
    if (!this._initialized) {
      return;
    }
    this._initialized = false;

    window.removeEventListener("unload", this.onWindowUnload, false);
    DownloadsCommon.getIndicatorData(window).removeView(this);

    // Reset the view properties, so that a neutral indicator is displayed if we
    // are visible only temporarily as an anchor.
    this.counter = "";
    this.percentComplete = 0;
    this.paused = false;
    this.attention = DownloadsCommon.ATTENTION_NONE;
  },

  /**
   * Ensures that the user interface elements required to display the indicator
   * are loaded, then invokes the given callback.
   */
  _ensureOperational(aCallback) {
    if (this._operational) {
      if (aCallback) {
        aCallback();
      }
      return;
    }

    // If we don't have a _placeholder, there's no chance that the overlay
    // will load correctly: bail (and don't set _operational to true!)
    if (!DownloadsButton._placeholder) {
      return;
    }

    DownloadsOverlayLoader.ensureOverlayLoaded(
      DownloadsButton.kIndicatorOverlay,
      () => {
        this._operational = true;

        // If the view is initialized, we need to update the elements now that
        // they are finally available in the document.
        if (this._initialized) {
          DownloadsCommon.getIndicatorData(window).refreshView(this);
        }

        if (aCallback) {
          aCallback();
        }
      });
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Direct control functions

  /**
   * Set while we are waiting for a notification to fade out.
   */
  _notificationTimeout: null,

  /**
   * Check if the panel containing aNode is open.
   * @param aNode
   *        the node whose panel we're interested in.
   */
  _isAncestorPanelOpen(aNode) {
    while (aNode && aNode.localName != "panel") {
      aNode = aNode.parentNode;
    }
    return aNode && aNode.state == "open";
  },

  /**
   * If the status indicator is visible in its assigned position, shows for a
   * brief time a visual notification of a relevant event, like a new download.
   *
   * @param aType
   *        Set to "start" for new downloads, "finish" for completed downloads.
   */
  showEventNotification(aType) {
    if (!this._initialized) {
      return;
    }

    if (!DownloadsCommon.animateNotifications) {
      return;
    }

    // No need to show visual notification if the panel is visible.
    if (DownloadsPanel.isPanelShowing) {
      return;
    }

    let anchor = DownloadsButton._placeholder;
    let widgetGroup = CustomizableUI.getWidget("downloads-button");
    let widget = widgetGroup.forWindow(window);
    if (widget.overflowed || widgetGroup.areaType == CustomizableUI.TYPE_MENU_PANEL) {
      if (anchor && this._isAncestorPanelOpen(anchor)) {
        // If the containing panel is open, don't do anything, because the
        // notification would appear under the open panel. See
        // https://bugzilla.mozilla.org/show_bug.cgi?id=984023
        return;
      }

      // Otherwise, try to use the anchor of the panel:
      anchor = widget.anchor;
    }
    if (!anchor || !isElementVisible(anchor.parentNode)) {
      // Our container isn't visible, so can't show the animation:
      return;
    }

    if (this._notificationTimeout) {
      clearTimeout(this._notificationTimeout);
    }

    // The notification element is positioned to show in the same location as
    // the downloads button. It's not in the downloads button itself in order to
    // be able to anchor the notification elsewhere if required, and to ensure
    // the notification isn't clipped by overflow properties of the anchor's
    // container.
    let notifier = this.notifier;
    if (notifier.style.transform == '') {
      let anchorRect = anchor.getBoundingClientRect();
      let notifierRect = notifier.getBoundingClientRect();
      let topDiff = anchorRect.top - notifierRect.top;
      let leftDiff = anchorRect.left - notifierRect.left;
      let heightDiff = anchorRect.height - notifierRect.height;
      let widthDiff = anchorRect.width - notifierRect.width;
      let translateX = (leftDiff + .5 * widthDiff) + "px";
      let translateY = (topDiff + .5 * heightDiff) + "px";
      notifier.style.transform = "translate(" +  translateX + ", " + translateY + ")";
    }
    notifier.setAttribute("notification", aType);
    this._notificationTimeout = setTimeout(() => {
      notifier.removeAttribute("notification");
      notifier.style.transform = '';
    }, 1000);
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Callback functions from DownloadsIndicatorData

  /**
   * Indicates whether the indicator should be shown because there are some
   * downloads to be displayed.
   */
  set hasDownloads(aValue) {
    if (this._hasDownloads != aValue || (!this._operational && aValue)) {
      this._hasDownloads = aValue;

      // If there is at least one download, ensure that the view elements are
      if (aValue) {
        this._ensureOperational();
      }
    }
    return aValue;
  },
  get hasDownloads() {
    return this._hasDownloads;
  },
  _hasDownloads: false,

  /**
   * Status text displayed in the indicator.  If this is set to an empty value,
   * then the small downloads icon is displayed instead of the text.
   */
  set counter(aValue) {
    if (!this._operational) {
      return this._counter;
    }

    if (this._counter !== aValue) {
      this._counter = aValue;
      if (this._counter)
        this.indicator.setAttribute("counter", "true");
      else
        this.indicator.removeAttribute("counter");
      // We have to set the attribute instead of using the property because the
      // XBL binding isn't applied if the element is invisible for any reason.
      this._indicatorCounter.setAttribute("value", aValue);
    }
    return aValue;
  },
  _counter: null,

  /**
   * Progress indication to display, from 0 to 100, or -1 if unknown.  The
   * progress bar is hidden if the current progress is unknown and no status
   * text is set in the "counter" property.
   */
  set percentComplete(aValue) {
    if (!this._operational) {
      return this._percentComplete;
    }

    if (this._percentComplete !== aValue) {
      this._percentComplete = aValue;
      if (this._percentComplete >= 0)
        this.indicator.setAttribute("progress", "true");
      else
        this.indicator.removeAttribute("progress");
      // We have to set the attribute instead of using the property because the
      // XBL binding isn't applied if the element is invisible for any reason.
      this._indicatorProgress.setAttribute("value", Math.max(aValue, 0));
    }
    return aValue;
  },
  _percentComplete: null,

  /**
   * Indicates whether the progress won't advance because of a paused state.
   * Setting this property forces a paused progress bar to be displayed, even if
   * the current progress information is unavailable.
   */
  set paused(aValue) {
    if (!this._operational) {
      return this._paused;
    }

    if (this._paused != aValue) {
      this._paused = aValue;
      if (this._paused) {
        this.indicator.setAttribute("paused", "true")
      } else {
        this.indicator.removeAttribute("paused");
      }
    }
    return aValue;
  },
  _paused: false,

  /**
   * Set when the indicator should draw user attention to itself.
   */
  set attention(aValue) {
    if (!this._operational) {
      return this._attention;
    }

    if (this._attention != aValue) {
      this._attention = aValue;

      // Check if the downloads button is in the menu panel, to determine which
      // button needs to get a badge.
      let widgetGroup = CustomizableUI.getWidget("downloads-button");
      let inMenu = widgetGroup.areaType == CustomizableUI.TYPE_MENU_PANEL;

      if (aValue == DownloadsCommon.ATTENTION_NONE) {
        this.indicator.removeAttribute("attention");
        if (inMenu) {
          gMenuButtonBadgeManager.removeBadge(gMenuButtonBadgeManager.BADGEID_DOWNLOAD);
        }
      } else {
        this.indicator.setAttribute("attention", aValue);
        if (inMenu) {
          let badgeClass = "download-" + aValue;
          gMenuButtonBadgeManager.addBadge(gMenuButtonBadgeManager.BADGEID_DOWNLOAD, badgeClass);
        }
      }
    }
    return aValue;
  },
  _attention: DownloadsCommon.ATTENTION_NONE,

  //////////////////////////////////////////////////////////////////////////////
  //// User interface event functions

  onWindowUnload() {
    // This function is registered as an event listener, we can't use "this".
    DownloadsIndicatorView.ensureTerminated();
  },

  onCommand(aEvent) {
    // If the downloads button is in the menu panel, open the Library
    let widgetGroup = CustomizableUI.getWidget("downloads-button");
    if (widgetGroup.areaType == CustomizableUI.TYPE_MENU_PANEL) {
      DownloadsPanel.showDownloadsHistory();
    } else {
      DownloadsPanel.showPanel();
    }

    aEvent.stopPropagation();
  },

  onDragOver(aEvent) {
    browserDragAndDrop.dragOver(aEvent);
  },

  onDrop(aEvent) {
    let dt = aEvent.dataTransfer;
    // If dragged item is from our source, do not try to
    // redownload already downloaded file.
    if (dt.mozGetDataAt("application/x-moz-file", 0))
      return;

    let name = {};
    let url = browserDragAndDrop.drop(aEvent, name);
    if (url) {
      if (url.startsWith("about:")) {
        return;
      }

      let sourceDoc = dt.mozSourceNode ? dt.mozSourceNode.ownerDocument : document;
      saveURL(url, name.value, null, true, true, null, sourceDoc);
      aEvent.preventDefault();
    }
  },

  _indicator: null,
  __indicatorCounter: null,
  __indicatorProgress: null,

  /**
   * Returns a reference to the main indicator element, or null if the element
   * is not present in the browser window yet.
   */
  get indicator() {
    if (this._indicator) {
      return this._indicator;
    }

    let indicator = document.getElementById("downloads-button");
    if (!indicator || indicator.getAttribute("indicator") != "true") {
      return null;
    }

    return this._indicator = indicator;
  },

  get indicatorAnchor() {
    let widget = CustomizableUI.getWidget("downloads-button")
                               .forWindow(window);
    if (widget.overflowed) {
      return widget.anchor;
    }
    return document.getElementById("downloads-indicator-anchor");
  },

  get _indicatorCounter() {
    return this.__indicatorCounter ||
      (this.__indicatorCounter = document.getElementById("downloads-indicator-counter"));
  },

  get _indicatorProgress() {
    return this.__indicatorProgress ||
      (this.__indicatorProgress = document.getElementById("downloads-indicator-progress"));
  },

  get notifier() {
    return this._notifier ||
      (this._notifier = document.getElementById("downloads-notification-anchor"));
  },

  _onCustomizedAway() {
    this._indicator = null;
    this.__indicatorCounter = null;
    this.__indicatorProgress = null;
  },

  afterCustomize() {
    // If the cached indicator is not the one currently in the document,
    // invalidate our references
    if (this._indicator != document.getElementById("downloads-button")) {
      this._onCustomizedAway();
      this._operational = false;
      this.ensureTerminated();
      this.ensureInitialized();
    }
  },
};

Object.defineProperty(this, "DownloadsIndicatorView", {
  value: DownloadsIndicatorView,
  enumerable: true,
  writable: false
});
