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

// Not sure where this comes from.  It's one of the possible values
// for the mousewheel.* preferences.
const MOUSE_SCROLL_TEXTSIZE = 3;

/**
 * Controls the "text zoom" setting and its site-specific preferences.
 */
var TextZoom = {

  //**************************************************************************//
  // Name & Values

  // The name of the setting.  Identifies the setting in the prefs database.
  name: "browser.content.text-zoom",

  // The global value (if any) for the setting.  Retrieved from the prefs
  // database when this handler gets initialized, then updated as it changes.
  // If there is no global value, then this should be undefined.
  globalValue: undefined,

  // From viewZoomOverlay.js
  minValue: 1,
  maxValue: 2000,
  defaultValue: 100,


  //**************************************************************************//
  // Convenience Getters

  __zoomManager: null,
  get _zoomManager() {
    if (!this.__zoomManager)
      this.__zoomManager = ZoomManager.prototype.getInstance();
    return this.__zoomManager;
  },

  // Content Pref Service
  __cps: null,
  get _cps() {
    if (!this.__cps)
      this.__cps = Cc["@mozilla.org/content-pref/service;1"].
                   getService(Ci.nsIContentPrefService);
    return this.__cps;
  },

  // Pref Branch
  __prefBranch: null,
  get _prefBranch() {
    if (!this.__prefBranch)
      this.__prefBranch = Cc["@mozilla.org/preferences-service;1"].
                           getService(Ci.nsIPrefBranch);
    return this.__prefBranch;
  },


  //**************************************************************************//
  // nsISupports

  // We can't use the Ci shortcut here because it isn't defined yet.
  interfaces: [Components.interfaces.nsIDOMEventListener,
               Components.interfaces.nsIContentPrefObserver,
               Components.interfaces.nsISupports],

  QueryInterface: function TextZoom_QueryInterface(aIID) {
    if (!this.interfaces.some( function(v) { return aIID.equals(v) } ))
      throw Cr.NS_ERROR_NO_INTERFACE;
    return this;
  },


  //**************************************************************************//
  // Initialization & Destruction

  init: function TextZoom_init() {
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

  destroy: function TextZoom_destroy() {
    ContentPrefSink.removeObserver(this.name, this);
    this._cps.removeObserver(this.name, this);
    window.removeEventListener("DOMMouseScroll", this, false);

    // Delete references to XPCOM components to make sure we don't leak them
    // (although we haven't observed leakage in tests).
    this.__cps = null;
    this.__prefBranch = null;
  },


  //**************************************************************************//
  // Event Handlers

  // nsIDOMEventListener

  handleEvent: function TextZoom_handleEvent(event) {
    // The only events we handle are DOMMouseScroll events.
    this._handleMouseScrolled(event);
  },

  _handleMouseScrolled: function TextZoom__handleMouseScrolled(event) {
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

    // Don't do anything if this isn't a "change text size" scroll event.
    if (this._getAppPref(pref, null) != MOUSE_SCROLL_TEXTSIZE)
      return;

    // XXX Lazily cache all the possible action prefs so we don't have to get
    // them anew from the pref service for every scroll event?  We'd have to
    // make sure to observe them so we can update the cache when they change.

    // We have to call _applySettingToPref in a timeout because we handle
    // the event before the event state manager has a chance to apply the zoom
    // during nsEventStateManager::PostHandleEvent.
    window.setTimeout(function() { TextZoom._applySettingToPref() }, 0);
  },

  // nsIContentPrefObserver

  onContentPrefSet: function TextZoom_onContentPrefSet(aGroup, aName, aValue) {
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

  onContentPrefRemoved: function TextZoom_onContentPrefRemoved(aGroup, aName) {
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

  onLocationChanged: function TextZoom_onLocationChanged(aURI, aName, aValue) {
    this._applyPrefToSetting(aValue);
  },


  //**************************************************************************//
  // Setting & Pref Manipulation

  reduce: function TextZoom_reduce() {
    this._zoomManager.reduce();
    this._applySettingToPref();
  },

  enlarge: function TextZoom_enlarge() {
    this._zoomManager.enlarge();
    this._applySettingToPref();
  },

  reset: function TextZoom_reset() {
    if (typeof this.globalValue != "undefined")
      this._zoomManager.textZoom = this.globalValue;
    else
      this._zoomManager.reset();

    this._removePref();
  },

  /**
   * Set the text zoom for the current tab.
   *
   * Per DocumentViewerImpl::SetTextZoom in nsDocumentViewer.cpp, it looks
   * like we can set the zoom to its current value without significant impact
   * on performance, as the setting is only applied if it differs from the
   * current setting.
   *
   * And perhaps we should always set the zoom even if it were to incur
   * a performance penalty, since SetTextZoom claims that child documents
   * may have a different zoom under unusual circumstances, and it implies
   * that those child zooms should get updated when the parent zoom gets set.
   *
   * So when we apply new zoom values to the browser, we simply set the zoom.
   * We don't check first to see if the new value is the same as the current
   * one.
   **/
  _applyPrefToSetting: function TextZoom__applyPrefToSetting(aValue) {
    // Bug 375918 means this will sometimes throw, so we catch it
    // and don't do anything in those cases.
    try {
      if (typeof aValue != "undefined")
        this._zoomManager.textZoom = this._ensureValid(aValue);
      else if (typeof this.globalValue != "undefined")
        this._zoomManager.textZoom = this.globalValue;
      else
        this._zoomManager.reset();
    }
    catch(ex) {}
  },

  _applySettingToPref: function TextZoom__applySettingToPref() {
    var textZoom = this._zoomManager.textZoom;
    this._cps.setPref(gBrowser.currentURI, this.name, textZoom);
  },

  _removePref: function TextZoom__removePref() {
    this._cps.removePref(gBrowser.currentURI, this.name);
  },


  //**************************************************************************//
  // Utilities

  _ensureValid: function TextZoom__ensureValid(aValue) {
    if (isNaN(aValue))
      return this.defaultValue;

    if (aValue < this.minValue)
      return this.minValue;

    if (aValue > this.maxValue)
      return this.maxValue;

    return aValue;
  },

  /**
   * Get a value from a pref or a default value if the pref doesn't exist.
   *
   * @param   aPrefName
   * @param   aDefaultValue
   * @returns the pref's value or the default (if it is missing)
   */
  _getAppPref: function TextZoom__getAppPref(aPrefName, aDefaultValue) {
    try {
      switch (this._prefBranch.getPrefType(aPrefName)) {
        case this._prefBranch.PREF_STRING:
          return this._prefBranch.getCharPref(aPrefName);

        case this._prefBranch.PREF_BOOL:
          return this._prefBranch.getBoolPref(aPrefName);

        case this._prefBranch.PREF_INT:
          return this._prefBranch.getIntPref(aPrefName);
      }
    }
    catch (ex) { /* return the default value */ }
    
    return aDefaultValue;
  }
};
