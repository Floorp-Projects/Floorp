/*
#ifdef 0
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
#endif
 */

// One of the possible values for the mousewheel.* preferences.
// From nsEventStateManager.cpp.
const MOUSE_SCROLL_ZOOM = 3;

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
  },

  destroy: function FullZoom_destroy() {
    gPrefService.removeObserver("browser.zoom.", this);
    this._cps2.removeObserverForName(this.name, this);
    window.removeEventListener("DOMMouseScroll", this, false);
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
      isZoomEvent = (gPrefService.getIntPref(pref) == MOUSE_SCROLL_ZOOM);
    } catch (e) {}
    if (!isZoomEvent)
      return;

    // XXX Lazily cache all the possible action prefs so we don't have to get
    // them anew from the pref service for every scroll event?  We'd have to
    // make sure to observe them so we can update the cache when they change.

    // We have to call _applySettingToPref in a timeout because we handle
    // the event before the event state manager has a chance to apply the zoom
    // during nsEventStateManager::PostHandleEvent.
    //
    // Since the zoom will have already been updated by the time
    // _applySettingToPref is called, pass true to suppress updating the zoom
    // again.  Note that this is not an optimization: due to asynchronicity of
    // the content preference service, the zoom may have been updated again by
    // the time that onContentPrefSet is called as a result of this call to
    // _applySettingToPref.
    window.setTimeout(function (self) self._applySettingToPref(true), 0, this);
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
    }
  },

  // nsIContentPrefObserver

  onContentPrefSet: function FullZoom_onContentPrefSet(aGroup, aName, aValue) {
    if (this._ignoreNextOnContentPrefSet) {
      delete this._ignoreNextOnContentPrefSet;
      return;
    }
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
    if (!gBrowser.currentURI)
      return;

    let domain = this._cps2.extractDomain(gBrowser.currentURI.spec);
    if (aGroup) {
      if (aGroup == domain)
        this._applyPrefToSetting(aValue);
      return;
    }

    this._globalValue = aValue === undefined ? aValue :
                          this._ensureValid(aValue);

    // If the current page doesn't have a site-specific preference, then its
    // zoom should be set to the new global preference now that the global
    // preference has changed.
    let hasPref = false;
    let ctxt = this._loadContextFromWindow(gBrowser.contentWindow);
    this._cps2.getByDomainAndName(gBrowser.currentURI.spec, this.name, ctxt, {
      handleResult: function () hasPref = true,
      handleCompletion: function () {
        if (!hasPref &&
            gBrowser.currentURI &&
            this._cps2.extractDomain(gBrowser.currentURI.spec) == domain)
          this._applyPrefToSetting();
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
    if (!aURI || (aIsTabSwitch && !this.siteSpecific)) {
      this._notifyOnLocationChange();
      return;
    }

    // Avoid the cps roundtrip and apply the default/global pref.
    if (aURI.spec == "about:blank") {
      this._applyPrefToSetting(undefined, aBrowser, function () {
        this._notifyOnLocationChange();
      }.bind(this));
      return;
    }

    let browser = aBrowser || gBrowser.selectedBrowser;

    // Media documents should always start at 1, and are not affected by prefs.
    if (!aIsTabSwitch && browser.contentDocument.mozSyntheticDocument) {
      ZoomManager.setZoomForBrowser(browser, 1);
      this._notifyOnLocationChange();
      return;
    }

    let ctxt = this._loadContextFromWindow(browser.contentWindow);
    let pref = this._cps2.getCachedByDomainAndName(aURI.spec, this.name, ctxt);
    if (pref) {
      this._applyPrefToSetting(pref.value, browser, function () {
        this._notifyOnLocationChange();
      }.bind(this));
      return;
    }

    let value = undefined;
    this._cps2.getByDomainAndName(aURI.spec, this.name, ctxt, {
      handleResult: function (resultPref) value = resultPref.value,
      handleCompletion: function () {
        if (browser.currentURI &&
            this._cps2.extractDomain(browser.currentURI.spec) ==
              this._cps2.extractDomain(aURI.spec)) {
          this._applyPrefToSetting(value, browser, function () {
            this._notifyOnLocationChange();
          }.bind(this));
        }
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
   *
   * @param callback  Optional.  If given, it's asynchronously called when the
   *                  zoom update completes.
   */
  reduce: function FullZoom_reduce(callback) {
    ZoomManager.reduce();
    this._applySettingToPref(false, callback);
  },

  /**
   * Enlarges the zoom level of the page in the current browser.
   *
   * @param callback  Optional.  If given, it's asynchronously called when the
   *                  zoom update completes.
   */
  enlarge: function FullZoom_enlarge(callback) {
    ZoomManager.enlarge();
    this._applySettingToPref(false, callback);
  },

  /**
   * Sets the zoom level of the page in the current browser to the global zoom
   * level.
   *
   * @param callback  Optional.  If given, it's asynchronously called when the
   *                  zoom update completes.
   */
  reset: function FullZoom_reset(callback) {
    this._getGlobalValue(gBrowser.contentWindow, function (value) {
      if (value === undefined)
        ZoomManager.reset();
      else
        ZoomManager.zoom = value;
      this._removePref(callback);
    });
  },

  /**
   * Set the zoom level for the current tab.
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
   * @param aBrowser   The browser containing the page whose zoom level is to be
   *                   set.  If falsey, the currently selected browser is used.
   * @param aCallback  Optional.  If given, it's asynchronously called when
   *                   complete.
   */
  _applyPrefToSetting: function FullZoom__applyPrefToSetting(aValue, aBrowser, aCallback) {
    if (!this.siteSpecific || gInPrintPreviewMode) {
      this._executeSoon(aCallback);
      return;
    }

    var browser = aBrowser || (gBrowser && gBrowser.selectedBrowser);
    if (browser.contentDocument.mozSyntheticDocument) {
      this._executeSoon(aCallback);
      return;
    }

    if (aValue !== undefined) {
      ZoomManager.setZoomForBrowser(browser, this._ensureValid(aValue));
      this._executeSoon(aCallback);
      return;
    }

    this._getGlobalValue(browser.contentWindow, function (value) {
      if (gBrowser.selectedBrowser == browser)
        ZoomManager.setZoomForBrowser(browser, value === undefined ? 1 : value);
      this._executeSoon(aCallback);
    });
  },

  /**
   * Saves the zoom level of the page in the current browser to the content
   * prefs store.
   *
   * @param suppressZoomChange  Because this method sets a content preference,
   *                            our onContentPrefSet method is called as a
   *                            result, which updates the current zoom level.
   *                            If suppressZoomChange is true, then the next
   *                            call to onContentPrefSet will not update the
   *                            zoom level.
   * @param callback            Optional.  If given, it's asynchronously called
   *                            when done.
   */
  _applySettingToPref: function FullZoom__applySettingToPref(suppressZoomChange, callback) {
    if (!this.siteSpecific ||
        gInPrintPreviewMode ||
        content.document.mozSyntheticDocument) {
      this._executeSoon(callback);
      return;
    }

    this._cps2.set(gBrowser.currentURI.spec, this.name, ZoomManager.zoom,
                   this._loadContextFromWindow(gBrowser.contentWindow), {
      handleCompletion: function () {
        if (suppressZoomChange)
          // The content preference service calls observers after callbacks, and
          // it calls both in the same turn of the event loop.  onContentPrefSet
          // will therefore be called next, in the same turn of the event loop,
          // and it will check this flag.
          this._ignoreNextOnContentPrefSet = true;
        if (callback)
          callback();
      }.bind(this)
    });
  },

  /**
   * Removes from the content prefs store the zoom level of the current browser.
   *
   * @param callback  Optional.  If given, it's asynchronously called when done.
   */
  _removePref: function FullZoom__removePref(callback) {
    if (content.document.mozSyntheticDocument) {
      this._executeSoon(callback);
      return;
    }
    let ctxt = this._loadContextFromWindow(gBrowser.contentWindow);
    this._cps2.removeByDomainAndName(gBrowser.currentURI.spec, this.name, ctxt, {
      handleCompletion: function () {
        if (callback)
          callback();
      }
    });
  },


  //**************************************************************************//
  // Utilities

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
   *                  bound to this object (FullZoom) and passed the preference
   *                  value.
   */
  _getGlobalValue: function FullZoom__getGlobalValue(window, callback) {
    // * !("_globalValue" in this) => global value not yet cached.
    // * this._globalValue === undefined => global value known not to exist.
    // * Otherwise, this._globalValue is a number, the global value.
    if ("_globalValue" in this) {
      callback.call(this, this._globalValue);
      return;
    }
    let value = undefined;
    this._cps2.getGlobal(this.name, this._loadContextFromWindow(window), {
      handleResult: function (pref) value = pref.value,
      handleCompletion: function () {
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
