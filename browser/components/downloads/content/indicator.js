/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-env mozilla/browser-window */

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

// DownloadsButton

/**
 * Main entry point for the downloads indicator.  Depending on how the toolbars
 * have been customized, this object determines if we should show a fully
 * functional indicator, a placeholder used during customization and in the
 * customization palette, or a neutral view as a temporary anchor for the
 * downloads panel.
 */
const DownloadsButton = {
  /**
   * Returns a reference to the downloads button position placeholder, or null
   * if not available because it has been removed from the toolbars.
   */
  get _placeholder() {
    return document.getElementById("downloads-button");
  },

  /**
   * Indicates whether toolbar customization is in progress.
   */
  _customizing: false,

  /**
   * Indicates whether the button has been torn down.
   * TODO: This is used for a temporary workaround for bug 1543537 and should be
   * removed when fixed.
   */
  _uninitialized: false,

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
   * Determines the position where the indicator should appear, and moves its
   * associated element to the new position.
   *
   * @return Anchor element, or null if the indicator is not visible.
   */
  _getAnchorInternal() {
    let indicator = DownloadsIndicatorView.indicator;
    if (!indicator) {
      // Exit now if the button is not in the document.
      return null;
    }

    indicator.open = this._anchorRequested;

    let widget = CustomizableUI.getWidget("downloads-button");
    // Determine if the indicator is located on an invisible toolbar.
    if (
      !isElementVisible(indicator.parentNode) &&
      widget.areaType == CustomizableUI.TYPE_TOOLBAR
    ) {
      return null;
    }

    return DownloadsIndicatorView.indicatorAnchor;
  },

  /**
   * Indicates whether we should try and show the indicator temporarily as an
   * anchor for the panel, even if the indicator would be hidden by default.
   */
  _anchorRequested: false,

  /**
   * Ensures that there is an anchor available for the panel.
   *
   * @return Anchor element where the panel should be anchored, or null if an
   *         anchor is not available (for example because both the tab bar and
   *         the navigation bar are hidden).
   */
  getAnchor() {
    // Do not allow anchoring the panel to the element while customizing.
    if (this._customizing) {
      return null;
    }

    this._anchorRequested = true;
    return this._getAnchorInternal();
  },

  /**
   * Allows the temporary anchor to be hidden.
   */
  releaseAnchor() {
    this._anchorRequested = false;
    this._getAnchorInternal();
  },

  /**
   * Unhide the button. Generally, this only needs to use the placeholder.
   * However, when starting customize mode, if the button is in the palette,
   * we need to unhide it before customize mode is entered, otherwise it
   * gets ignored by customize mode. To do this, we pass true for
   * `includePalette`. We don't always look in the palette because it's
   * inefficient (compared to getElementById), shouldn't be necessary, and
   * if _placeholder returned the node even if in the palette, other checks
   * would break.
   *
   * @param includePalette  whether to search the palette, too. Defaults to false.
   */
  unhide(includePalette = false) {
    let button = this._placeholder;
    if (!button && includePalette) {
      button = gNavToolbox.palette.querySelector("#downloads-button");
    }
    if (button && button.hasAttribute("hidden")) {
      button.removeAttribute("hidden");
      if (this._navBar.contains(button)) {
        this._navBar.setAttribute("downloadsbuttonshown", "true");
      }
    }
  },

  hide() {
    let button = this._placeholder;
    if (this.autoHideDownloadsButton && button && button.closest("toolbar")) {
      DownloadsPanel.hidePanel();
      button.setAttribute("hidden", "true");
      this._navBar.removeAttribute("downloadsbuttonshown");
    }
  },

  startAutoHide() {
    if (DownloadsIndicatorView.hasDownloads) {
      this.unhide();
    } else {
      this.hide();
    }
  },

  checkForAutoHide() {
    if (this._uninitialized) {
      return;
    }
    let button = this._placeholder;
    if (
      !this._customizing &&
      this.autoHideDownloadsButton &&
      button &&
      button.closest("toolbar")
    ) {
      this.startAutoHide();
    } else {
      this.unhide();
    }
  },

  // Callback from CustomizableUI when nodes get moved around.
  // We use this to track whether our node has moved somewhere
  // where we should (not) autohide it.
  onWidgetAfterDOMChange(node) {
    if (node == this._placeholder) {
      this.checkForAutoHide();
    }
  },

  /**
   * This function is called when toolbar customization starts.
   *
   * During customization, we never show the actual download progress indication
   * or the event notifications, but we show a neutral placeholder.  The neutral
   * placeholder is an ordinary button defined in the browser window that can be
   * moved freely between the toolbars and the customization palette.
   */
  onCustomizeStart(win) {
    if (win == window) {
      // Prevent the indicator from being displayed as a temporary anchor
      // during customization, even if requested using the getAnchor method.
      this._customizing = true;
      this._anchorRequested = false;
      this.unhide(true);
    }
  },

  onCustomizeEnd(win) {
    if (win == window) {
      this._customizing = false;
      this.checkForAutoHide();
      DownloadsIndicatorView.afterCustomize();
    }
  },

  init() {
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "autoHideDownloadsButton",
      "browser.download.autohideButton",
      true,
      this.checkForAutoHide.bind(this)
    );

    CustomizableUI.addListener(this);
    this.checkForAutoHide();
  },

  uninit() {
    this._uninitialized = true;
    CustomizableUI.removeListener(this);
  },

  get _tabsToolbar() {
    delete this._tabsToolbar;
    return (this._tabsToolbar = document.getElementById("TabsToolbar"));
  },

  get _navBar() {
    delete this._navBar;
    return (this._navBar = document.getElementById("nav-bar"));
  },
};

Object.defineProperty(this, "DownloadsButton", {
  value: DownloadsButton,
  enumerable: true,
  writable: false,
});

// DownloadsIndicatorView

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

    window.addEventListener("unload", this.onWindowUnload);
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

    window.removeEventListener("unload", this.onWindowUnload);
    DownloadsCommon.getIndicatorData(window).removeView(this);

    // Reset the view properties, so that a neutral indicator is displayed if we
    // are visible only temporarily as an anchor.
    this.percentComplete = 0;
    this.attention = DownloadsCommon.ATTENTION_NONE;
  },

  /**
   * Ensures that the user interface elements required to display the indicator
   * are loaded.
   */
  _ensureOperational() {
    if (this._operational) {
      return;
    }

    // If we don't have a _placeholder, there's no chance that everything
    // will load correctly: bail (and don't set _operational to true!)
    if (!DownloadsButton._placeholder) {
      return;
    }

    this._operational = true;

    // If the view is initialized, we need to update the elements now that
    // they are finally available in the document.
    if (this._initialized) {
      DownloadsCommon.getIndicatorData(window).refreshView(this);
    }
  },

  // Direct control functions

  /**
   * Set to the type ("start" or "finish") when display of a notification is in-progress
   */
  _currentNotificationType: null,

  /**
   * Set to the type ("start" or "finish") when a notification arrives while we
   * are waiting for the timeout of the previous notification
   */
  _nextNotificationType: null,

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
   * Display or enqueue a visual notification of a relevant event, like a new download.
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

    // enqueue this notification while the current one is being displayed
    if (this._currentNotificationType) {
      // only queue up the notification if it is different to the current one
      if (this._currentNotificationType != aType) {
        this._nextNotificationType = aType;
      }
    } else {
      this._showNotification(aType);
    }
  },

  /**
   * If the status indicator is visible in its assigned position, shows for a
   * brief time a visual notification of a relevant event, like a new download.
   *
   * @param aType
   *        Set to "start" for new downloads, "finish" for completed downloads.
   */
  _showNotification(aType) {
    // No need to show visual notification if the panel is visible.
    if (DownloadsPanel.isPanelShowing) {
      return;
    }

    let anchor = DownloadsButton._placeholder;
    let widgetGroup = CustomizableUI.getWidget("downloads-button");
    let widget = widgetGroup.forWindow(window);
    if (widgetGroup.areaType == CustomizableUI.TYPE_MENU_PANEL) {
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

    // The notification element is positioned to show in the same location as
    // the downloads button. It's not in the downloads button itself in order to
    // be able to anchor the notification elsewhere if required, and to ensure
    // the notification isn't clipped by overflow properties of the anchor's
    // container.
    // Note: no notifier animation for download finished in Photon
    let notifier = this.notifier;

    if (aType == "start") {
      // Show the notifier before measuring for size/placement. Being hidden by default
      // avoids the interference with scrolling/APZ when the notifier element is
      // tall enough to overlap the tabbrowser element
      notifier.removeAttribute("hidden");

      // the anchor height may vary if font-size is changed or
      // compact/tablet mode is selected so recalculate this each time
      let anchorRect = anchor.getBoundingClientRect();
      let notifierRect = notifier.getBoundingClientRect();
      let topDiff = anchorRect.top - notifierRect.top;
      let leftDiff = anchorRect.left - notifierRect.left;
      let heightDiff = anchorRect.height - notifierRect.height;
      let widthDiff = anchorRect.width - notifierRect.width;
      let translateX = leftDiff + 0.5 * widthDiff + "px";
      let translateY = topDiff + 0.5 * heightDiff + "px";
      notifier.style.transform =
        "translate(" + translateX + ", " + translateY + ")";
      notifier.setAttribute("notification", aType);
    }
    anchor.setAttribute("notification", aType);

    let animationDuration;
    // This value is determined by the overall duration of animation in CSS.
    animationDuration = aType == "start" ? 760 : 850;

    this._currentNotificationType = aType;

    setTimeout(() => {
      requestAnimationFrame(() => {
        notifier.setAttribute("hidden", "true");
        notifier.removeAttribute("notification");
        notifier.style.transform = "";
        anchor.removeAttribute("notification");

        requestAnimationFrame(() => {
          let nextType = this._nextNotificationType;
          this._currentNotificationType = null;
          this._nextNotificationType = null;
          if (nextType) {
            this._showNotification(nextType);
          }
        });
      });
    }, animationDuration);
  },

  // Callback functions from DownloadsIndicatorData

  /**
   * Indicates whether the indicator should be shown because there are some
   * downloads to be displayed.
   */
  set hasDownloads(aValue) {
    if (this._hasDownloads != aValue || (!this._operational && aValue)) {
      this._hasDownloads = aValue;

      // If there is at least one download, ensure that the view elements are
      // operational
      if (aValue) {
        DownloadsButton.unhide();
        this._ensureOperational();
      } else {
        DownloadsButton.checkForAutoHide();
      }
    }
    return aValue;
  },
  get hasDownloads() {
    return this._hasDownloads;
  },
  _hasDownloads: false,

  /**
   * Progress indication to display, from 0 to 100, or -1 if unknown.
   * Progress is not visible if the current progress is unknown.
   */
  set percentComplete(aValue) {
    if (!this._operational) {
      return this._percentComplete;
    }

    if (this._percentComplete !== aValue) {
      this._percentComplete = aValue;
      this._refreshAttention();

      if (this._percentComplete >= 0) {
        this.indicator.setAttribute("progress", "true");
        // For arrow type only:
        // We set animationDelay to a minus value (0s ~ -100s) to show the
        // corresponding frame needed for progress.
        this._progressIcon.style.animationDelay = -this._percentComplete + "s";
      } else {
        this.indicator.removeAttribute("progress");
        this._progressIcon.style.animationDelay = "1s";
      }
    }
    return aValue;
  },
  _percentComplete: null,

  /**
   * Set when the indicator should draw user attention to itself.
   */
  set attention(aValue) {
    if (!this._operational) {
      return this._attention;
    }
    if (this._attention != aValue) {
      this._attention = aValue;
      this._refreshAttention();
    }
    return this._attention;
  },

  _refreshAttention() {
    // Check if the downloads button is in the menu panel, to determine which
    // button needs to get a badge.
    let widgetGroup = CustomizableUI.getWidget("downloads-button");
    let inMenu = widgetGroup.areaType == CustomizableUI.TYPE_MENU_PANEL;

    // For arrow-Styled indicator, suppress success attention if we have
    // progress in toolbar
    let suppressAttention =
      !inMenu &&
      this._attention == DownloadsCommon.ATTENTION_SUCCESS &&
      this._percentComplete >= 0;

    if (
      suppressAttention ||
      this._attention == DownloadsCommon.ATTENTION_NONE
    ) {
      this.indicator.removeAttribute("attention");
    } else {
      this.indicator.setAttribute("attention", this._attention);
    }
  },
  _attention: DownloadsCommon.ATTENTION_NONE,

  // User interface event functions

  onWindowUnload() {
    // This function is registered as an event listener, we can't use "this".
    DownloadsIndicatorView.ensureTerminated();
  },

  onCommand(aEvent) {
    if (
      // On Mac, ctrl-click will send a context menu event from the widget, so
      // we don't want to bring up the panel when ctrl key is pressed.
      (aEvent.type == "mousedown" &&
        (aEvent.button != 0 ||
          (AppConstants.platform == "macosx" && aEvent.ctrlKey))) ||
      (aEvent.type == "keypress" && aEvent.key != " " && aEvent.key != "Enter")
    ) {
      return;
    }

    DownloadsPanel.showPanel();
    aEvent.stopPropagation();
  },

  onDragOver(aEvent) {
    browserDragAndDrop.dragOver(aEvent);
  },

  onDrop(aEvent) {
    let dt = aEvent.dataTransfer;
    // If dragged item is from our source, do not try to
    // redownload already downloaded file.
    if (dt.mozGetDataAt("application/x-moz-file", 0)) {
      return;
    }

    let links = browserDragAndDrop.dropLinks(aEvent);
    if (!links.length) {
      return;
    }
    let sourceDoc = dt.mozSourceNode
      ? dt.mozSourceNode.ownerDocument
      : document;
    let handled = false;
    for (let link of links) {
      if (link.url.startsWith("about:")) {
        continue;
      }
      saveURL(link.url, link.name, null, true, true, null, sourceDoc);
      handled = true;
    }
    if (handled) {
      aEvent.preventDefault();
    }
  },

  _indicator: null,
  __progressIcon: null,

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

    return (this._indicator = indicator);
  },

  get indicatorAnchor() {
    let widgetGroup = CustomizableUI.getWidget("downloads-button");
    if (widgetGroup.areaType == CustomizableUI.TYPE_MENU_PANEL) {
      let overflowIcon = widgetGroup.forWindow(window).anchor;
      return overflowIcon.icon;
    }

    return this.indicator.badgeStack;
  },

  get _progressIcon() {
    return (
      this.__progressIcon ||
      (this.__progressIcon = document.getElementById(
        "downloads-indicator-progress-inner"
      ))
    );
  },

  get notifier() {
    return (
      this._notifier ||
      (this._notifier = document.getElementById(
        "downloads-notification-anchor"
      ))
    );
  },

  _onCustomizedAway() {
    this._indicator = null;
    this.__progressIcon = null;
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
  writable: false,
});
