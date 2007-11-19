/*
#ifdef 0
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Content Preferences (cpref).
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Myk Melez <myk@mozilla.org>
 *   Dão Gottwald <dao@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
#endif
 */

// From nsMouseScrollEvent::kIsHorizontal
const MOUSE_SCROLL_IS_HORIZONTAL = 1 << 2;

// One of the possible values for the mousewheel.* preferences.
// From nsEventStateManager.cpp.
const MOUSE_SCROLL_FULLZOOM = 5;

/**
 * Controls the "full zoom" setting and its site-specific preferences.
 */
var FullZoom = {

  //**************************************************************************//
  // Name & Values

  // The name of the setting.  Identifies the setting in the prefs database.
  name: "browser.content.full-zoom",

  // The global value (if any) for the setting.  Retrieved from the prefs
  // database when this handler gets initialized, then updated as it changes.
  // If there is no global value, then this should be undefined.
  globalValue: undefined,


  //**************************************************************************//
  // Convenience Getters

  // Content Pref Service
  get _cps() {
    delete this._cps;
    return this._cps = Cc["@mozilla.org/content-pref/service;1"].
                       getService(Ci.nsIContentPrefService);
  },


  //**************************************************************************//
  // nsISupports

  // We can't use the Ci shortcut here because it isn't defined yet.
  interfaces: [Components.interfaces.nsIDOMEventListener,
               Components.interfaces.nsIContentPrefObserver,
               Components.interfaces.nsISupports],

  QueryInterface: function (aIID) {
    if (!this.interfaces.some(function (v) aIID.equals(v)))
      throw Cr.NS_ERROR_NO_INTERFACE;
    return this;
  },


  //**************************************************************************//
  // Initialization & Destruction

  init: function () {
    // Listen for scrollwheel events so we can save scrollwheel-based changes.
    window.addEventListener("DOMMouseScroll", this, false);

    // Register ourselves with the service so we know when our pref changes.
    this._cps.addObserver(this.name, this);

    // Register ourselves with the sink so we know when the location changes.
    var globalValue = ContentPrefSink.addObserver(this.name, this);
    this.globalValue = this._ensureValid(globalValue);

    // Set the initial value of the setting.
    this._applyPrefToSetting();
  },

  destroy: function () {
    ContentPrefSink.removeObserver(this.name, this);
    this._cps.removeObserver(this.name, this);
    window.removeEventListener("DOMMouseScroll", this, false);

    // Delete references to XPCOM components to make sure we don't leak them
    // (although we haven't observed leakage in tests).
    for (var i in this) {
      try { this[i] = null }
      // Ignore "setting a property that has only a getter" exceptions.
      catch(ex) {}
    }
  },


  //**************************************************************************//
  // Event Handlers

  // nsIDOMEventListener

  handleEvent: function (event) {
    switch (event.type) {
      case "DOMMouseScroll":
        this._handleMouseScrolled(event);
        break;
    }
  },

  _handleMouseScrolled: function (event) {
    // Construct the "mousewheel action" pref key corresponding to this event.
    // Based on nsEventStateManager::GetBasePrefKeyForMouseWheel.
    var pref = "mousewheel";
    if (event.scrollFlags & MOUSE_SCROLL_IS_HORIZONTAL)
      pref += ".horizscroll";

    if (event.shiftKey)
      pref += ".withshiftkey";
    else if (event.ctrlKey)
      pref += ".withcontrolkey";
    else if (event.altKey)
      pref += ".withaltkey";
    else if (event.metaKey)
      pref += ".withmetakey";
    else
      pref += ".withnokey";

    pref += ".action";

    // Don't do anything if this isn't a "zoom" scroll event.
    var isZoomEvent = false;
    try {
      isZoomEvent = (gPrefService.getIntPref(pref) == MOUSE_SCROLL_FULLZOOM);
    } catch (e) {}
    if (!isZoomEvent)
      return;

    // XXX Lazily cache all the possible action prefs so we don't have to get
    // them anew from the pref service for every scroll event?  We'd have to
    // make sure to observe them so we can update the cache when they change.

    // We have to call _applySettingToPref in a timeout because we handle
    // the event before the event state manager has a chance to apply the zoom
    // during nsEventStateManager::PostHandleEvent.
    window.setTimeout(function (self) { self._applySettingToPref() }, 0, this);
  },

  // nsIContentPrefObserver

  onContentPrefSet: function (aGroup, aName, aValue) {
    if (aGroup == this._cps.grouper.group(gBrowser.currentURI))
      this._applyPrefToSetting(aValue);
    else if (aGroup == null) {
      this.globalValue = this._ensureValid(aValue);

      // If the current page doesn't have a site-specific preference,
      // then its zoom should be set to the new global preference now that
      // the global preference has changed.
      if (!this._cps.hasPref(gBrowser.currentURI, this.name))
        this._applyPrefToSetting();
    }
  },

  onContentPrefRemoved: function (aGroup, aName) {
    if (aGroup == this._cps.grouper.group(gBrowser.currentURI))
      this._applyPrefToSetting();
    else if (aGroup == null) {
      this.globalValue = undefined;

      // If the current page doesn't have a site-specific preference,
      // then its zoom should be set to the default preference now that
      // the global preference has changed.
      if (!this._cps.hasPref(gBrowser.currentURI, this.name))
        this._applyPrefToSetting();
    }
  },

  // ContentPrefSink observer

  onLocationChanged: function (aURI, aName, aValue) {
    this._applyPrefToSetting(aValue);
  },


  //**************************************************************************//
  // Setting & Pref Manipulation

  reduce: function () {
    ZoomManager.reduce();
    this._applySettingToPref();
  },

  enlarge: function () {
    ZoomManager.enlarge();
    this._applySettingToPref();
  },

  reset: function () {
    if (typeof this.globalValue != "undefined")
      ZoomManager.fullZoom = this.globalValue;
    else
      ZoomManager.reset();

    this._removePref();
  },

  setSettingValue: function () {
    var value = this._cps.getPref(gBrowser.currentURI, this.name);
    this._applyPrefToSetting(value);
  },

  /**
   * Set the zoom level for the current tab.
   *
   * Per DocumentViewerImpl::SetFullZoom in nsDocumentViewer.cpp, it looks
   * like we can set the zoom to its current value without significant impact
   * on performance, as the setting is only applied if it differs from the
   * current setting.
   *
   * And perhaps we should always set the zoom even if it were to incur
   * a performance penalty, since SetFullZoom claims that child documents
   * may have a different zoom under unusual circumstances, and it implies
   * that those child zooms should get updated when the parent zoom gets set.
   *
   * So when we apply new zoom values to the browser, we simply set the zoom.
   * We don't check first to see if the new value is the same as the current
   * one.
   **/
  _applyPrefToSetting: function (aValue) {
    if (gInPrintPreviewMode)
      return;

    // Bug 375918 means this will sometimes throw, so we catch it
    // and don't do anything in those cases.
    try {
      if (typeof aValue != "undefined")
        ZoomManager.fullZoom = this._ensureValid(aValue);
      else if (typeof this.globalValue != "undefined")
        ZoomManager.fullZoom = this.globalValue;
      else
        ZoomManager.reset();
    }
    catch(ex) {}
  },

  _applySettingToPref: function () {
    if (gInPrintPreviewMode)
      return;

    var fullZoom = ZoomManager.fullZoom;
    this._cps.setPref(gBrowser.currentURI, this.name, fullZoom);
  },

  _removePref: function () {
    this._cps.removePref(gBrowser.currentURI, this.name);
  },


  //**************************************************************************//
  // Utilities

  _ensureValid: function (aValue) {
    if (isNaN(aValue))
      return 1;

    if (aValue < ZoomManager.MIN)
      return ZoomManager.MIN;

    if (aValue > ZoomManager.MAX)
      return ZoomManager.MAX;

    return aValue;
  }
};
