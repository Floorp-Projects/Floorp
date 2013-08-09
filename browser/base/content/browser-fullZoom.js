/*
#ifdef 0
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
#endif
 */

/**
 * Controls the "full zoom" setting and its site-specific preferences.
 */
var FullZoom = {
  // Identifies the setting in the content prefs database.
  name: "browser.content.full-zoom",

  // browser.zoom.siteSpecific preference cache
  _siteSpecificPref: undefined,

  // browser.zoom.updateBackgroundTabs preference cache
  updateBackgroundTabs: undefined,

  // One of the possible values for the mousewheel.* preferences.
  // From nsEventStateManager.h.
  ACTION_ZOOM: 3,

  // This maps browser outer window IDs to monotonically increasing integer
  // tokens.  _browserTokenMap[outerID] is increased each time the zoom is
  // changed in the browser whose outer window ID is outerID.  See
  // _getBrowserToken and _ignorePendingZoomAccesses.
  _browserTokenMap: new Map(),

  get siteSpecific() {
    return this._siteSpecificPref;
  },

  //**************************************************************************//
  // nsISupports

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMEventListener,
                                         Ci.nsIObserver,
                                         Ci.nsIContentPrefObserver,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsISupports]),

  //**************************************************************************//
  // Initialization & Destruction

  init: function FullZoom_init() {
    // Bug 691614 - zooming support for electrolysis
    if (gMultiProcessBrowser)
      return;

    // Listen for scrollwheel events so we can save scrollwheel-based changes.
    window.addEventListener("DOMMouseScroll", this, false);

    // Register ourselves with the service so we know when our pref changes.
    this._cps2 = Cc["@mozilla.org/content-pref/service;1"].
                 getService(Ci.nsIContentPrefService2);
    this._cps2.addObserverForName(this.name, this);

    this._siteSpecificPref =
      gPrefService.getBoolPref("browser.zoom.siteSpecific");
    this.updateBackgroundTabs =
      gPrefService.getBoolPref("browser.zoom.updateBackgroundTabs");
    // Listen for changes to the browser.zoom branch so we can enable/disable
    // updating background tabs and per-site saving and restoring of zoom levels.
    gPrefService.addObserver("browser.zoom.", this, true);

    Services.obs.addObserver(this, "outer-window-destroyed", false);
  },

  destroy: function FullZoom_destroy() {
    // Bug 691614 - zooming support for electrolysis
    if (gMultiProcessBrowser)
      return;

    gPrefService.removeObserver("browser.zoom.", this);
    this._cps2.removeObserverForName(this.name, this);
    window.removeEventListener("DOMMouseScroll", this, false);
    Services.obs.removeObserver(this, "outer-window-destroyed");
  },


  //**************************************************************************//
  // Event Handlers

  // nsIDOMEventListener

  handleEvent: function FullZoom_handleEvent(event) {
    switch (event.type) {
      case "DOMMouseScroll":
        this._handleMouseScrolled(event);
        break;
    }
  },

  _handleMouseScrolled: function FullZoom__handleMouseScrolled(event) {
    // Construct the "mousewheel action" pref key corresponding to this event.
    // Based on nsEventStateManager::WheelPrefs::GetBasePrefName().
    var pref = "mousewheel.";

    var pressedModifierCount = event.shiftKey + event.ctrlKey + event.altKey +
                                 event.metaKey + event.getModifierState("OS");
    if (pressedModifierCount != 1) {
      pref += "default.";
    } else if (event.shiftKey) {
      pref += "with_shift.";
    } else if (event.ctrlKey) {
      pref += "with_control.";
    } else if (event.altKey) {
      pref += "with_alt.";
    } else if (event.metaKey) {
      pref += "with_meta.";
    } else {
      pref += "with_win.";
    }

    pref += "action";

    // Don't do anything if this isn't a "zoom" scroll event.
    var isZoomEvent = false;
    try {
      isZoomEvent = (gPrefService.getIntPref(pref) == this.ACTION_ZOOM);
    } catch (e) {}
    if (!isZoomEvent)
      return;

    // XXX Lazily cache all the possible action prefs so we don't have to get
    // them anew from the pref service for every scroll event?  We'd have to
    // make sure to observe them so we can update the cache when they change.

    // We have to call _applyZoomToPref in a timeout because we handle the
    // event before the event state manager has a chance to apply the zoom
    // during nsEventStateManager::PostHandleEvent.
    let browser = gBrowser.selectedBrowser;
    let token = this._getBrowserToken(browser);
    window.setTimeout(function () {
      if (token.isCurrent)
        this._applyZoomToPref(browser);
    }.bind(this), 0);
  },

  // nsIObserver

  observe: function (aSubject, aTopic, aData) {
    switch (aTopic) {
      case "nsPref:changed":
        switch (aData) {
          case "browser.zoom.siteSpecific":
            this._siteSpecificPref =
              gPrefService.getBoolPref("browser.zoom.siteSpecific");
            break;
          case "browser.zoom.updateBackgroundTabs":
            this.updateBackgroundTabs =
              gPrefService.getBoolPref("browser.zoom.updateBackgroundTabs");
            break;
        }
        break;
      case "outer-window-destroyed":
        let outerID = aSubject.QueryInterface(Ci.nsISupportsPRUint64).data;
        this._browserTokenMap.delete(outerID);
        break;
    }
  },

  // nsIContentPrefObserver

  onContentPrefSet: function FullZoom_onContentPrefSet(aGroup, aName, aValue) {
    this._onContentPrefChanged(aGroup, aValue);
  },

  onContentPrefRemoved: function FullZoom_onContentPrefRemoved(aGroup, aName) {
    this._onContentPrefChanged(aGroup, undefined);
  },

  /**
   * Appropriately updates the zoom level after a content preference has
   * changed.
   *
   * @param aGroup  The group of the changed preference.
   * @param aValue  The new value of the changed preference.  Pass undefined to
   *                indicate the preference's removal.
   */
  _onContentPrefChanged: function FullZoom__onContentPrefChanged(aGroup, aValue) {
    if (this._isNextContentPrefChangeInternal) {
      // Ignore changes that FullZoom itself makes.  This works because the
      // content pref service calls callbacks before notifying observers, and it
      // does both in the same turn of the event loop.
      delete this._isNextContentPrefChangeInternal;
      return;
    }

    let browser = gBrowser.selectedBrowser;
    if (!browser.currentURI)
      return;

    let domain = this._cps2.extractDomain(browser.currentURI.spec);
    if (aGroup) {
      if (aGroup == domain)
        this._applyPrefToZoom(aValue, browser);
      return;
    }

    this._globalValue = aValue === undefined ? aValue :
                          this._ensureValid(aValue);

    // If the current page doesn't have a site-specific preference, then its
    // zoom should be set to the new global preference now that the global
    // preference has changed.
    let hasPref = false;
    let ctxt = this._loadContextFromWindow(browser.contentWindow);
    let token = this._getBrowserToken(browser);
    this._cps2.getByDomainAndName(browser.currentURI.spec, this.name, ctxt, {
      handleResult: function () hasPref = true,
      handleCompletion: function () {
        if (!hasPref && token.isCurrent)
          this._applyPrefToZoom(undefined, browser);
      }.bind(this)
    });
  },

  // location change observer

  /**
   * Called when the location of a tab changes.
   * When that happens, we need to update the current zoom level if appropriate.
   *
   * @param aURI
   *        A URI object representing the new location.
   * @param aIsTabSwitch
   *        Whether this location change has happened because of a tab switch.
   * @param aBrowser
   *        (optional) browser object displaying the document
   */
  onLocationChange: function FullZoom_onLocationChange(aURI, aIsTabSwitch, aBrowser) {
    // Bug 691614 - zooming support for electrolysis
    if (gMultiProcessBrowser)
      return;

    // Ignore all pending async zoom accesses in the browser.  Pending accesses
    // that started before the location change will be prevented from applying
    // to the new location.
    let browser = aBrowser || gBrowser.selectedBrowser;
    this._ignorePendingZoomAccesses(browser);

    // Bug 691614 - zooming support for electrolysis
    if (gMultiProcessBrowser)
      return;

    if (!aURI || (aIsTabSwitch && !this.siteSpecific)) {
      this._notifyOnLocationChange();
      return;
    }

    // Avoid the cps roundtrip and apply the default/global pref.
    if (aURI.spec == "about:blank") {
      this._applyPrefToZoom(undefined, browser,
                            this._notifyOnLocationChange.bind(this));
      return;
    }

    // Media documents should always start at 1, and are not affected by prefs.
    if (!aIsTabSwitch && browser.contentDocument.mozSyntheticDocument) {
      ZoomManager.setZoomForBrowser(browser, 1);
      // _ignorePendingZoomAccesses already called above, so no need here.
      this._notifyOnLocationChange();
      return;
    }

    // See if the zoom pref is cached.
    let ctxt = this._loadContextFromWindow(browser.contentWindow);
    let pref = this._cps2.getCachedByDomainAndName(aURI.spec, this.name, ctxt);
    if (pref) {
      this._applyPrefToZoom(pref.value, browser,
                            this._notifyOnLocationChange.bind(this));
      return;
    }

    // It's not cached, so we have to asynchronously fetch it.
    let value = undefined;
    let token = this._getBrowserToken(browser);
    this._cps2.getByDomainAndName(aURI.spec, this.name, ctxt, {
      handleResult: function (resultPref) value = resultPref.value,
      handleCompletion: function () {
        if (!token.isCurrent) {
          this._notifyOnLocationChange();
          return;
        }
        this._applyPrefToZoom(value, browser,
                              this._notifyOnLocationChange.bind(this));
      }.bind(this)
    });
  },

  // update state of zoom type menu item

  updateMenu: function FullZoom_updateMenu() {
    var menuItem = document.getElementById("toggle_zoom");

    menuItem.setAttribute("checked", !ZoomManager.useFullZoom);
  },

  //**************************************************************************//
  // Setting & Pref Manipulation

  /**
   * Reduces the zoom level of the page in the current browser.
   */
  reduce: function FullZoom_reduce() {
    ZoomManager.reduce();
    let browser = gBrowser.selectedBrowser;
    this._ignorePendingZoomAccesses(browser);
    this._applyZoomToPref(browser);
  },

  /**
   * Enlarges the zoom level of the page in the current browser.
   */
  enlarge: function FullZoom_enlarge() {
    ZoomManager.enlarge();
    let browser = gBrowser.selectedBrowser;
    this._ignorePendingZoomAccesses(browser);
    this._applyZoomToPref(browser);
  },

  /**
   * Sets the zoom level of the page in the current browser to the global zoom
   * level.
   */
  reset: function FullZoom_reset() {
    let browser = gBrowser.selectedBrowser;
    let token = this._getBrowserToken(browser);
    this._getGlobalValue(browser.contentWindow, function (value) {
      if (token.isCurrent) {
        ZoomManager.setZoomForBrowser(browser, value === undefined ? 1 : value);
        this._ignorePendingZoomAccesses(browser);
      }
    });
    this._removePref(browser);
  },

  /**
   * Set the zoom level for a given browser.
   *
   * Per nsPresContext::setFullZoom, we can set the zoom to its current value
   * without significant impact on performance, as the setting is only applied
   * if it differs from the current setting.  In fact getting the zoom and then
   * checking ourselves if it differs costs more.
   *
   * And perhaps we should always set the zoom even if it was more expensive,
   * since nsDocumentViewer::SetTextZoom claims that child documents can have
   * a different text zoom (although it would be unusual), and it implies that
   * those child text zooms should get updated when the parent zoom gets set,
   * and perhaps the same is true for full zoom
   * (although nsDocumentViewer::SetFullZoom doesn't mention it).
   *
   * So when we apply new zoom values to the browser, we simply set the zoom.
   * We don't check first to see if the new value is the same as the current
   * one.
   *
   * @param aValue     The zoom level value.
   * @param aBrowser   The zoom is set in this browser.  Required.
   * @param aCallback  If given, it's asynchronously called when complete.
   */
  _applyPrefToZoom: function FullZoom__applyPrefToZoom(aValue, aBrowser, aCallback) {
    if (!this.siteSpecific || gInPrintPreviewMode) {
      this._executeSoon(aCallback);
      return;
    }

    // aBrowser.contentDocument is sometimes gone because this method is called
    // by content pref service callbacks, which themselves can be called at any
    // time, even after browsers are closed.
    if (!aBrowser.contentDocument ||
        aBrowser.contentDocument.mozSyntheticDocument) {
      this._executeSoon(aCallback);
      return;
    }

    if (aValue !== undefined) {
      ZoomManager.setZoomForBrowser(aBrowser, this._ensureValid(aValue));
      this._ignorePendingZoomAccesses(aBrowser);
      this._executeSoon(aCallback);
      return;
    }

    let token = this._getBrowserToken(aBrowser);
    this._getGlobalValue(aBrowser.contentWindow, function (value) {
      if (token.isCurrent) {
        ZoomManager.setZoomForBrowser(aBrowser, value === undefined ? 1 : value);
        this._ignorePendingZoomAccesses(aBrowser);
      }
      this._executeSoon(aCallback);
    });
  },

  /**
   * Saves the zoom level of the page in the given browser to the content
   * prefs store.
   *
   * @param browser  The zoom of this browser will be saved.  Required.
   */
  _applyZoomToPref: function FullZoom__applyZoomToPref(browser) {
    if (!this.siteSpecific ||
        gInPrintPreviewMode ||
        browser.contentDocument.mozSyntheticDocument)
      return;

    this._cps2.set(browser.currentURI.spec, this.name,
                   ZoomManager.getZoomForBrowser(browser),
                   this._loadContextFromWindow(browser.contentWindow), {
      handleCompletion: function () {
        this._isNextContentPrefChangeInternal = true;
      }.bind(this),
    });
  },

  /**
   * Removes from the content prefs store the zoom level of the given browser.
   *
   * @param browser  The zoom of this browser will be removed.  Required.
   */
  _removePref: function FullZoom__removePref(browser) {
    if (browser.contentDocument.mozSyntheticDocument)
      return;
    let ctxt = this._loadContextFromWindow(browser.contentWindow);
    this._cps2.removeByDomainAndName(browser.currentURI.spec, this.name, ctxt, {
      handleCompletion: function () {
        this._isNextContentPrefChangeInternal = true;
      }.bind(this),
    });
  },

  //**************************************************************************//
  // Utilities

  /**
   * Returns the zoom change token of the given browser.  Asynchronous
   * operations that access the given browser's zoom should use this method to
   * capture the token before starting and use token.isCurrent to determine if
   * it's safe to access the zoom when done.  If token.isCurrent is false, then
   * after the async operation started, either the browser's zoom was changed or
   * the browser was destroyed, and depending on what the operation is doing, it
   * may no longer be safe to set and get its zoom.
   *
   * @param browser  The token of this browser will be returned.
   * @return  An object with an "isCurrent" getter.
   */
  _getBrowserToken: function FullZoom__getBrowserToken(browser) {
    let outerID = this._browserOuterID(browser);
    let map = this._browserTokenMap;
    if (!map.has(outerID))
      map.set(outerID, 0);
    return {
      token: map.get(outerID),
      get isCurrent() {
        // At this point, the browser may have been destructed and unbound but
        // its outer ID not removed from the map because outer-window-destroyed
        // hasn't been received yet.  In that case, the browser is unusable, it
        // has no properties, so return false.  Check for this case by getting a
        // property, say, docShell.
        return map.get(outerID) === this.token && browser.docShell;
      },
    };
  },

  /**
   * Increments the zoom change token for the given browser so that pending
   * async operations know that it may be unsafe to access they zoom when they
   * finish.
   *
   * @param browser  Pending accesses in this browser will be ignored.
   */
  _ignorePendingZoomAccesses: function FullZoom__ignorePendingZoomAccesses(browser) {
    let outerID = this._browserOuterID(browser);
    let map = this._browserTokenMap;
    map.set(outerID, (map.get(outerID) || 0) + 1);
  },

  _browserOuterID: function FullZoom__browserOuterID(browser) {
    return browser.
           contentWindow.
           QueryInterface(Ci.nsIInterfaceRequestor).
           getInterface(Ci.nsIDOMWindowUtils).
           outerWindowID;
  },

  _ensureValid: function FullZoom__ensureValid(aValue) {
    // Note that undefined is a valid value for aValue that indicates a known-
    // not-to-exist value.
    if (isNaN(aValue))
      return 1;

    if (aValue < ZoomManager.MIN)
      return ZoomManager.MIN;

    if (aValue > ZoomManager.MAX)
      return ZoomManager.MAX;

    return aValue;
  },

  /**
   * Gets the global browser.content.full-zoom content preference.
   *
   * WARNING: callback may be called synchronously or asynchronously.  The
   * reason is that it's usually desirable to avoid turns of the event loop
   * where possible, since they can lead to visible, jarring jumps in zoom
   * level.  It's not always possible to avoid them, though.  As a convenience,
   * then, this method takes a callback and returns nothing.
   *
   * @param window    The content window pertaining to the zoom.
   * @param callback  Synchronously or asynchronously called when done.  It's
   *                  bound to this object (FullZoom) and called as:
   *                    callback(prefValue)
   */
  _getGlobalValue: function FullZoom__getGlobalValue(window, callback) {
    // * !("_globalValue" in this) => global value not yet cached.
    // * this._globalValue === undefined => global value known not to exist.
    // * Otherwise, this._globalValue is a number, the global value.
    if ("_globalValue" in this) {
      callback.call(this, this._globalValue, true);
      return;
    }
    let value = undefined;
    this._cps2.getGlobal(this.name, this._loadContextFromWindow(window), {
      handleResult: function (pref) value = pref.value,
      handleCompletion: function (reason) {
        this._globalValue = this._ensureValid(value);
        callback.call(this, this._globalValue);
      }.bind(this)
    });
  },

  /**
   * Gets the load context from the given window.
   *
   * @param window  The window whose load context will be returned.
   * @return        The nsILoadContext of the given window.
   */
  _loadContextFromWindow: function FullZoom__loadContextFromWindow(window) {
    return window.
           QueryInterface(Ci.nsIInterfaceRequestor).
           getInterface(Ci.nsIWebNavigation).
           QueryInterface(Ci.nsILoadContext);
  },

  /**
   * Asynchronously broadcasts a "browser-fullZoom:locationChange" notification
   * so that tests can select tabs, load pages, etc. and be notified when the
   * zoom levels on those pages change.  The notification is always asynchronous
   * so that observers are guaranteed a consistent behavior.
   */
  _notifyOnLocationChange: function FullZoom__notifyOnLocationChange() {
    this._executeSoon(function () {
      Services.obs.notifyObservers(null, "browser-fullZoom:locationChange", "");
    });
  },

  _executeSoon: function FullZoom__executeSoon(callback) {
    if (!callback)
      return;
    Services.tm.mainThread.dispatch(callback, Ci.nsIThread.DISPATCH_NORMAL);
  },
};
