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

/**
 * Listens for site-specific pref-related events and dispatches them
 * to setting controllers.
 */

var ContentPrefSink = {
  //**************************************************************************//
  // Convenience Getters

  // Content Pref Service
  __cps: null,
  get _cps() {
    if (!this.__cps)
      this.__cps = Cc["@mozilla.org/content-pref/service;1"].
                   getService(Ci.nsIContentPrefService);
    return this.__cps;
  },


  //**************************************************************************//
  // nsISupports
  
  // Note: we can't use the Ci shortcut here because it isn't defined yet.
  interfaces: [Components.interfaces.nsIDOMEventListener,
               Components.interfaces.nsISupports],

  QueryInterface: function ContentPrefSink_QueryInterface(iid) {
    if (!this.interfaces.some( function(v) { return iid.equals(v) } ))
      throw Cr.NS_ERROR_NO_INTERFACE;
    return this;
  },


  //**************************************************************************//
  // Initialization & Destruction

  init: function ContentPrefSink_init() {
    gBrowser.addEventListener("DOMContentLoaded", this, false);
    // XXX Should we also listen for pageshow and/or load?
  },

  destroy: function ContentPrefSink_destroy() {
    gBrowser.removeEventListener("DOMContentLoaded", this, false);

    // Delete references to XPCOM components to make sure we don't leak them
    // (although we haven't observed leakage in tests).  Also delete references
    // in _observers and _genericObservers to avoid cycles with those that
    // refer to us and don't remove themselves from those observer pools.
    for (var i in this) {
      try { this[i] = null }
      // Ignore "setting a property that has only a getter" exceptions.
      catch(ex) {}
    }
  },


  //**************************************************************************//
  // Event Handlers

  // nsIDOMEventListener

  handleEvent: function ContentPrefSink_handleEvent(event) {
    // The only events we handle are DOMContentLoaded events.
    this._handleDOMContentLoaded(event);
  },

  handleLocationChanged: function ContentPrefSink_handleLocationChanged(uri) {
    if (!uri)
      return;

    var prefs = this._cps.getPrefs(uri);

    for (var name in this._observers) {
      var value = prefs.hasKey(name) ? prefs.get(name) : undefined;
      for each (var observer in this._observers[name])
        if (observer.onLocationChanged) {
          try {
            observer.onLocationChanged(uri, name, value);
          }
          catch(ex) {
            Components.utils.reportError(ex);
          }
        }
    }

    for each (var observer in this._genericObservers)
      if (observer.onLocationChanged) {
        try {
          observer.onLocationChanged(uri, prefs);
        }
        catch(ex) {
          Components.utils.reportError(ex);
        }
      }
  },

  _handleDOMContentLoaded: function ContentPrefSink__handleDOMContentLoaded(event) {
    var browser = gBrowser.getBrowserForDocument(event.target);
    // If the document isn't one of the ones loaded into a top-level browser,
    // don't notify observers about it.  XXX Might some observers want to know
    // about documents loaded into iframes?
    if (!browser)
      return;
    var uri = browser.currentURI;

    var prefs = this._cps.getPrefs(uri);

    for (var name in this._observers) {
      var value = prefs.hasKey(name) ? prefs.get(name) : undefined;
      for each (var observer in this._observers[name]) {
        if (observer.onDOMContentLoaded) {
          try {
            observer.onDOMContentLoaded(event, name, value);
          }
          catch(ex) {
            Components.utils.reportError(ex);
          }
        }
      }
    }

    for each (var observer in this._genericObservers) {
      if (observer.onDOMContentLoaded) {
        try {
          observer.onDOMContentLoaded(event, prefs);
        }
        catch(ex) {
          Components.utils.reportError(ex);
        }
      }
    }
  },


  //**************************************************************************//
  // Sink Observers

  _observers: {},
  _genericObservers: [],

  // Note: this isn't an XPCOM interface (for performance and so we can pass
  // nsIVariants), although it resembles nsIObserverService.  Sink observers
  // are regular JS objects, and they get called directly, not via XPConnect.

  /**
   * Add an observer.
   * 
   * The observer can be setting-specific or generic, which affects what it
   * gets handed when the sink notifies it about an event.  A setting-specific
   * observer gets handed the value of its pref for the target page, while
   * a generic observer gets handed the values of all prefs for the target page.
   *
   * @param    name      the name of the setting for which to add an observer,
   *                     or null to add a generic observer
   * @param    observer  the observer to add
   * 
   * @returns  if setting-specific, the global preference for the setting
   *           if generic, null
   */
  addObserver: function ContentPrefSink_addObserver(name, observer) {
    var observers;
    if (name) {
      if (!this._observers[name])
        this._observers[name] = [];
      observers = this._observers[name];
    }
    else
      observers = this._genericObservers;

    if (observers.indexOf(observer) == -1)
      observers.push(observer);

    return name ? this._cps.getPref(null, name) : null;
  },

  /**
   * Remove an observer.
   *
   * @param    name      the name of the setting for which to remove
   *                     an observer, or null to remove a generic observer
   * @param    observer  the observer to remove
   */
  removeObserver: function ContentPrefSink_removeObserver(name, observer) {
    var observers = name ? this._observers[name] : this._genericObservers;
    if (observers.indexOf(observer) != -1)
      observers.splice(observers.indexOf(observer), 1);
  },

  _getObservers: function ContentPrefSink__getObservers(name) {
    var observers = [];

    // Construct the list of observers, putting setting-specific ones before
    // generic ones, so observers that initialize individual settings (like
    // the page style controller) execute before observers that do something
    // with multiple settings and depend on them being initialized first
    // (f.e. the content prefs sidebar).
    if (name && this._observers[name])
      observers = observers.concat(this._observers[name]);
    observers = observers.concat(this._genericObservers);

    return observers;
  }
};
