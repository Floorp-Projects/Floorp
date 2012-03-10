/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
   * Returns a reference to the downloads button position placeholder, or null
   * if not available because it has been removed from the toolbars.
   */
  get _placeholder()
  {
    return document.getElementById("downloads-button");
  },

  /**
   * This function is called synchronously at window initialization.  It only
   * sets the visibility of user interface elements to avoid flickering.
   *
   * NOTE: To keep startup time to a minimum, this function should not perform
   *       any expensive operations or input/output, and should not cause the
   *       Download Manager service to start.
   */
  initializePlaceholder: function DB_initializePlaceholder()
  {
    // We must hide the placeholder used for toolbar customization, unless it
    // has been removed from the toolbars and is now located in the palette.
    let placeholder = this._placeholder;
    if (placeholder) {
      placeholder.collapsed = true;
    }
  },

  /**
   * This function is called asynchronously just after window initialization.
   *
   * NOTE: This function should limit the input/output it performs to improve
   *       startup time, and in particular should not cause the Download Manager
   *       service to start.
   */
  initializeIndicator: function DB_initializeIndicator()
  {
    this._update();
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
  customizeStart: function DB_customizeStart()
  {
    // Hide the indicator and prevent it to be displayed as a temporary anchor
    // during customization, even if requested using the getAnchor method.
    this._customizing = true;
    this._anchorRequested = false;
    DownloadsIndicatorView.indicator.collapsed = true;

    let placeholder = this._placeholder;
    if (placeholder) {
      placeholder.collapsed = false;
    }
  },

  /**
   * This function is called when toolbar customization ends.
   */
  customizeDone: function DB_customizeDone()
  {
    this._customizing = false;
    this._update();
  },

  /**
   * This function is called during initialization or when toolbar customization
   * ends.  It determines if we should enable or disable the object that keeps
   * the indicator updated, and ensures that the placeholder is hidden unless it
   * has been moved to the customization palette.
   *
   * NOTE: This function is also called on startup, thus it should limit the
   *       input/output it performs, and in particular should not cause the
   *       Download Manager service to start.
   */
  _update: function DB_update() {
    this._updatePositionInternal();

    let placeholder = this._placeholder;
    if (!DownloadsCommon.useToolkitUI) {
      DownloadsIndicatorView.ensureInitialized();
      if (placeholder) {
        placeholder.collapsed = true;
      }
    } else {
      DownloadsIndicatorView.ensureTerminated();
    }
  },

  /**
   * Determines the position where the indicator should appear, and moves its
   * associated element to the new position.
   *
   * @return Anchor element, or null if the indicator is not visible.
   */
  updatePosition: function DB_updatePosition()
  {
    // Don't update the position of the indicator if it's currently being used
    // as the anchor for the panel.  This ensures that the panel doesn't flicker
    // because we move the DOM element to which it's anchored.
    if (!this._anchorRequested) {
      return this._updatePositionInternal();
    }
  },

  _updatePositionInternal: function DB_updatePositionInternal()
  {
    let indicator = DownloadsIndicatorView.indicator;
    let placeholder = this._placeholder;

    // Firstly, determine if we should always hide the indicator.
    if (!placeholder && !this._anchorRequested &&
        !DownloadsIndicatorView.hasDownloads) {
      indicator.collapsed = true;
      return null;
    }
    indicator.collapsed = false;

    indicator.open = this._anchorRequested;

    // Determine if we should display the indicator in a known position.
    if (placeholder) {
      placeholder.parentNode.insertBefore(indicator, placeholder);
      // Determine if the placeholder is located on a visible toolbar.
      if (isElementVisible(placeholder.parentNode)) {
        return DownloadsIndicatorView.indicatorAnchor;
      }
    }

    // If not customized, the indicator is normally in the navigation bar.
    // Always place it in the default position, unless we need an anchor.
    if (!this._anchorRequested) {
      this._navBar.appendChild(indicator);
      return null;
    }

    // Show the indicator temporarily in the navigation bar, if visible.
    if (isElementVisible(this._navBar)) {
      this._navBar.appendChild(indicator);
      return DownloadsIndicatorView.indicatorAnchor;
    }

    // Show the indicator temporarily in the tab bar, if visible.
    if (!this._tabsToolbar.collapsed) {
      this._tabsToolbar.appendChild(indicator);
      return DownloadsIndicatorView.indicatorAnchor;
    }

    // The temporary anchor cannot be shown.
    return null;
  },

  /**
   * Indicates whether we should try and show the indicator temporarily as an
   * anchor for the panel, even if the indicator would be hidden by default.
   */
  _anchorRequested: false,

  /**
   * Ensures that there is an anchor available for the panel.
   *
   * @return Element where the panel should be anchored, or null if an anchor
   *         is not available, for example because both the tab bar and the
   *         navigation bar are hidden.
   */
  getAnchor: function DB_getAnchor()
  {
    // Do not allow anchoring the panel to the element while customizing.
    if (this._customizing) {
      return null;
    }

    this._anchorRequested = true;
    return this._updatePositionInternal();
  },

  /**
   * Allows the temporary anchor to be hidden.
   */
  releaseAnchor: function DB_releaseAnchor()
  {
    this._anchorRequested = false;
    this._updatePositionInternal();
  },

  get _tabsToolbar()
  {
    delete this._tabsToolbar;
    return this._tabsToolbar = document.getElementById("TabsToolbar");
  },

  get _navBar()
  {
    delete this._navBar;
    return this._navBar = document.getElementById("nav-bar");
  }
};

////////////////////////////////////////////////////////////////////////////////
//// DownloadsIndicatorView

/**
 * Builds and updates the actual downloads status widget, responding to changes
 * in the global status data, or provides a neutral view if the indicator is
 * removed from the toolbars and only used as a temporary anchor.  In addition,
 * handles the user interaction events raised by the widget.
 */
const DownloadsIndicatorView = {
  _initialized: false,

  /**
   * Prepares the downloads indicator to be displayed.
   */
  ensureInitialized: function DIV_ensureInitialized()
  {
    if (this._initialized) {
      return;
    }
    this._initialized = true;

    window.addEventListener("unload", this.onWindowUnload, false);
    DownloadsCommon.indicatorData.addView(this);
  },

  /**
   * Frees the internal resources related to the indicator.
   */
  ensureTerminated: function DIV_ensureTerminated()
  {
    if (!this._initialized) {
      return;
    }
    this._initialized = false;

    window.removeEventListener("unload", this.onWindowUnload, false);
    DownloadsCommon.indicatorData.removeView(this);

    // Reset the view properties, so that a neutral indicator is displayed if we
    // are visible only temporarily as an anchor.
    this.counter = "";
    this.percentComplete = 0;
    this.paused = false;
    this.attention = false;
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Direct control functions

  /**
   * Set while we are waiting for a notification to fade out.
   */
  _notificationTimeout: null,

  /**
   * If the status indicator is visible in its assigned position, shows for a
   * brief time a visual notification of a relevant event, like a new download.
   */
  showEventNotification: function DIV_showEventNotification()
  {
    if (!this._initialized) {
      return;
    }

    if (this._notificationTimeout) {
      clearTimeout(this._notificationTimeout);
    }

    let indicator = this.indicator;
    indicator.setAttribute("notification", "true");
    this._notificationTimeout = setTimeout(
      function () indicator.removeAttribute("notification"), 1000);
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Callback functions from DownloadsIndicatorData

  /**
   * Indicates whether the indicator should be shown because there are some
   * downloads to be displayed.
   */
  set hasDownloads(aValue)
  {
    if (this._hasDownloads != aValue) {
      this._hasDownloads = aValue;
      DownloadsButton.updatePosition();
    }
    return aValue;
  },
  get hasDownloads()
  {
    return this._hasDownloads;
  },
  _hasDownloads: false,

  /**
   * Status text displayed in the indicator.  If this is set to an empty value,
   * then the small downloads icon is displayed instead of the text.
   */
  set counter(aValue)
  {
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
  set percentComplete(aValue)
  {
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
  set paused(aValue)
  {
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
  set attention(aValue)
  {
    if (this._attention != aValue) {
      this._attention = aValue;
      if (aValue) {
        this.indicator.setAttribute("attention", "true")
      } else {
        this.indicator.removeAttribute("attention");
      }
    }
    return aValue;
  },
  _attention: false,

  //////////////////////////////////////////////////////////////////////////////
  //// User interface event functions

  onWindowUnload: function DIV_onWindowUnload()
  {
    // This function is registered as an event listener, we can't use "this".
    DownloadsIndicatorView.ensureTerminated();
  },

  onCommand: function DIV_onCommand(aEvent)
  {
    if (DownloadsCommon.useToolkitUI) {
      // The panel won't suppress attention for us, we need to clear now.
      DownloadsCommon.indicatorData.attention = false;
    }

    BrowserDownloadsUI();

    aEvent.stopPropagation();
  },

  onDragOver: function DIV_onDragOver(aEvent)
  {
    browserDragAndDrop.dragOver(aEvent);
  },

  onDragExit: function () { },

  onDrop: function DIV_onDrop(aEvent)
  {
    let name = {};
    let url = browserDragAndDrop.drop(aEvent, name);
    if (url) {
      saveURL(url, name.value, null, true, true);
      aEvent.preventDefault();
    }
  },

  get indicator()
  {
    delete this.indicator;
    return this.indicator = document.getElementById("downloads-indicator");
  },

  get indicatorAnchor()
  {
    delete this.indicatorAnchor;
    return this.indicatorAnchor =
      document.getElementById("downloads-indicator-anchor");
  },

  get _indicatorCounter()
  {
    delete this._indicatorCounter;
    return this._indicatorCounter =
      document.getElementById("downloads-indicator-counter");
  },

  get _indicatorProgress()
  {
    delete this._indicatorProgress;
    return this._indicatorProgress =
      document.getElementById("downloads-indicator-progress");
  }
};
