/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

  // This maps the browser to monotonically increasing integer
  // tokens. _browserTokenMap[browser] is increased each time the zoom is
  // changed in the browser. See _getBrowserToken and _ignorePendingZoomAccesses.
  _browserTokenMap: new WeakMap(),

  // Stores initial locations if we receive onLocationChange
  // events before we're initialized.
  _initialLocations: new WeakMap(),

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
    gBrowser.addEventListener("ZoomChangeUsingMouseWheel", this);

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

    // If we received onLocationChange events for any of the current browsers
    // before we were initialized we want to replay those upon initialization.
    for (let browser of gBrowser.browsers) {
      if (this._initialLocations.has(browser)) {
        this.onLocationChange(...this._initialLocations.get(browser), browser);
      }
    }

    // This should be nulled after initialization.
    this._initialLocations = null;
  },

  destroy: function FullZoom_destroy() {
    gPrefService.removeObserver("browser.zoom.", this);
    this._cps2.removeObserverForName(this.name, this);
    gBrowser.removeEventListener("ZoomChangeUsingMouseWheel", this);
  },


  //**************************************************************************//
  // Event Handlers

  // nsIDOMEventListener

  handleEvent: function FullZoom_handleEvent(event) {
    switch (event.type) {
      case "ZoomChangeUsingMouseWheel":
        let browser = this._getTargetedBrowser(event);
        this._ignorePendingZoomAccesses(browser);
        this._applyZoomToPref(browser);
        break;
    }
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

  onContentPrefSet: function FullZoom_onContentPrefSet(aGroup, aName, aValue, aIsPrivate) {
    this._onContentPrefChanged(aGroup, aValue, aIsPrivate);
  },

  onContentPrefRemoved: function FullZoom_onContentPrefRemoved(aGroup, aName, aIsPrivate) {
    this._onContentPrefChanged(aGroup, undefined, aIsPrivate);
  },

  /**
   * Appropriately updates the zoom level after a content preference has
   * changed.
   *
   * @param aGroup  The group of the changed preference.
   * @param aValue  The new value of the changed preference.  Pass undefined to
   *                indicate the preference's removal.
   */
  _onContentPrefChanged: function FullZoom__onContentPrefChanged(aGroup, aValue, aIsPrivate) {
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

    let ctxt = this._loadContextFromBrowser(browser);
    let domain = this._cps2.extractDomain(browser.currentURI.spec);
    if (aGroup) {
      if (aGroup == domain && ctxt.usePrivateBrowsing == aIsPrivate)
        this._applyPrefToZoom(aValue, browser);
      return;
    }

    this._globalValue = aValue === undefined ? aValue :
                          this._ensureValid(aValue);

    // If the current page doesn't have a site-specific preference, then its
    // zoom should be set to the new global preference now that the global
    // preference has changed.
    let hasPref = false;
    let token = this._getBrowserToken(browser);
    this._cps2.getByDomainAndName(browser.currentURI.spec, this.name, ctxt, {
      handleResult: function () { hasPref = true; },
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
    let browser = aBrowser || gBrowser.selectedBrowser;

    // If we haven't been initialized yet but receive an onLocationChange
    // notification then let's store and replay it upon initialization.
    if (this._initialLocations) {
      this._initialLocations.set(browser, [aURI, aIsTabSwitch]);
      return;
    }

    // Ignore all pending async zoom accesses in the browser.  Pending accesses
    // that started before the location change will be prevented from applying
    // to the new location.
    this._ignorePendingZoomAccesses(browser);

    if (!aURI || (aIsTabSwitch && !this.siteSpecific)) {
      this._notifyOnLocationChange(browser);
      return;
    }

    // Avoid the cps roundtrip and apply the default/global pref.
    if (aURI.spec == "about:blank") {
      this._applyPrefToZoom(undefined, browser,
                            this._notifyOnLocationChange.bind(this, browser));
      return;
    }

    // Media documents should always start at 1, and are not affected by prefs.
    if (!aIsTabSwitch && browser.isSyntheticDocument) {
      ZoomManager.setZoomForBrowser(browser, 1);
      // _ignorePendingZoomAccesses already called above, so no need here.
      this._notifyOnLocationChange(browser);
      return;
    }

    // See if the zoom pref is cached.
    let ctxt = this._loadContextFromBrowser(browser);
    let pref = this._cps2.getCachedByDomainAndName(aURI.spec, this.name, ctxt);
    if (pref) {
      this._applyPrefToZoom(pref.value, browser,
                            this._notifyOnLocationChange.bind(this, browser));
      return;
    }

    // It's not cached, so we have to asynchronously fetch it.
    let value = undefined;
    let token = this._getBrowserToken(browser);
    this._cps2.getByDomainAndName(aURI.spec, this.name, ctxt, {
      handleResult: function (resultPref) { value = resultPref.value; },
      handleCompletion: function () {
        if (!token.isCurrent) {
          this._notifyOnLocationChange(browser);
          return;
        }
        this._applyPrefToZoom(value, browser,
                              this._notifyOnLocationChange.bind(this, browser));
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
   * Sets the zoom level for the given browser to the given floating
   * point value, where 1 is the default zoom level.
   */
  setZoom: function (value, browser = gBrowser.selectedBrowser) {
    ZoomManager.setZoomForBrowser(browser, value);
    this._ignorePendingZoomAccesses(browser);
    this._applyZoomToPref(browser);
  },

  /**
   * Sets the zoom level of the page in the given browser to the global zoom
   * level.
   *
   * @return A promise which resolves when the zoom reset has been applied.
   */
  reset: function FullZoom_reset(browser = gBrowser.selectedBrowser) {
    let token = this._getBrowserToken(browser);
    let result = this._getGlobalValue(browser).then(value => {
      if (token.isCurrent) {
        ZoomManager.setZoomForBrowser(browser, value === undefined ? 1 : value);
        this._ignorePendingZoomAccesses(browser);
        Services.obs.notifyObservers(browser, "browser-fullZoom:zoomReset", "");
      }
    });
    this._removePref(browser);
    return result;
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

    // The browser is sometimes half-destroyed because this method is called
    // by content pref service callbacks, which themselves can be called at any
    // time, even after browsers are closed.
    if (!aBrowser.parentNode || aBrowser.isSyntheticDocument) {
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
    this._getGlobalValue(aBrowser).then(value => {
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
    Services.obs.notifyObservers(browser, "browser-fullZoom:zoomChange", "");
    if (!this.siteSpecific ||
        gInPrintPreviewMode ||
        browser.isSyntheticDocument)
      return;

    this._cps2.set(browser.currentURI.spec, this.name,
                   ZoomManager.getZoomForBrowser(browser),
                   this._loadContextFromBrowser(browser), {
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
    Services.obs.notifyObservers(browser, "browser-fullZoom:zoomReset", "");
    if (browser.isSyntheticDocument)
      return;
    let ctxt = this._loadContextFromBrowser(browser);
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
    let map = this._browserTokenMap;
    if (!map.has(browser))
      map.set(browser, 0);
    return {
      token: map.get(browser),
      get isCurrent() {
        // At this point, the browser may have been destructed and unbound but
        // its outer ID not removed from the map because outer-window-destroyed
        // hasn't been received yet.  In that case, the browser is unusable, it
        // has no properties, so return false.  Check for this case by getting a
        // property, say, docShell.
        return map.get(browser) === this.token && browser.parentNode;
      },
    };
  },

  /**
   * Returns the browser that the supplied zoom event is associated with.
   * @param event  The ZoomChangeUsingMouseWheel event.
   * @return  The associated browser element, if one exists, otherwise null.
   */
  _getTargetedBrowser: function FullZoom__getTargetedBrowser(event) {
    let target = event.originalTarget;

    // With remote content browsers, the event's target is the browser
    // we're looking for.
    const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    if (target instanceof window.XULElement &&
        target.localName == "browser" &&
        target.namespaceURI == XUL_NS)
      return target;

    // With in-process content browsers, the event's target is the content
    // document.
    if (target.nodeType == Node.DOCUMENT_NODE)
      return gBrowser.getBrowserForDocument(target);

    throw new Error("Unexpected ZoomChangeUsingMouseWheel event source");
  },

  /**
   * Increments the zoom change token for the given browser so that pending
   * async operations know that it may be unsafe to access they zoom when they
   * finish.
   *
   * @param browser  Pending accesses in this browser will be ignored.
   */
  _ignorePendingZoomAccesses: function FullZoom__ignorePendingZoomAccesses(browser) {
    let map = this._browserTokenMap;
    map.set(browser, (map.get(browser) || 0) + 1);
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
   * @param browser   The browser pertaining to the zoom.
   * @returns Promise<prefValue>
   *                  Resolves to the preference value when done.
   */
  _getGlobalValue: function FullZoom__getGlobalValue(browser) {
    // * !("_globalValue" in this) => global value not yet cached.
    // * this._globalValue === undefined => global value known not to exist.
    // * Otherwise, this._globalValue is a number, the global value.
    return new Promise(resolve => {
      if ("_globalValue" in this) {
        resolve(this._globalValue);
        return;
      }
      let value = undefined;
      this._cps2.getGlobal(this.name, this._loadContextFromBrowser(browser), {
        handleResult: function (pref) { value = pref.value; },
        handleCompletion: (reason) => {
          this._globalValue = this._ensureValid(value);
          resolve(this._globalValue);
        }
      });
    });
  },

  /**
   * Gets the load context from the given Browser.
   *
   * @param Browser  The Browser whose load context will be returned.
   * @return        The nsILoadContext of the given Browser.
   */
  _loadContextFromBrowser: function FullZoom__loadContextFromBrowser(browser) {
    return browser.loadContext;
  },

  /**
   * Asynchronously broadcasts "browser-fullZoom:location-change" so that
   * listeners can be notified when the zoom levels on those pages change.
   * The notification is always asynchronous so that observers are guaranteed a
   * consistent behavior.
   */
  _notifyOnLocationChange: function FullZoom__notifyOnLocationChange(browser) {
    this._executeSoon(function () {
      Services.obs.notifyObservers(browser, "browser-fullZoom:location-change", "");
    });
  },

  _executeSoon: function FullZoom__executeSoon(callback) {
    if (!callback)
      return;
    Services.tm.mainThread.dispatch(callback, Ci.nsIThread.DISPATCH_NORMAL);
  },
};
