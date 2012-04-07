/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Cu = Components.utils;
let Ci = Components.interfaces;
let Cc = Components.classes;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const NS_PREFBRANCH_PREFCHANGE_TOPIC_ID = "nsPref:changed";
const BROWSER_FRAMES_ENABLED_PREF = "dom.mozBrowserFramesEnabled";

/**
 * The BrowserElementAPI implements <iframe mozbrowser>.
 *
 * We detect windows and docshells contained inside <iframe mozbrowser>s and
 * alter their behavior so that the page inside the iframe can't tell that it's
 * framed and the page outside the iframe can observe changes within the iframe
 * (e.g. loadstart/loadstart, locationchange).
 */

function BrowserElementAPI() {}
BrowserElementAPI.prototype = {
  classID: Components.ID("{5d6fcab3-6c12-4db6-80fb-352df7a41602}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  /**
   * The keys of this map are the set of chrome event handlers we've observed
   * which contain a mozbrowser window.
   *
   * The values in this map are ignored.
   */
  _chromeEventHandlersWatching: new WeakMap(),

  /**
   * The keys of this map are the set of windows we've observed that are
   * directly contained in <iframe mozbrowser>s.
   *
   * The values in this map are ignored.
   */
  _topLevelBrowserWindows: new WeakMap(),

  _browserFramesPrefEnabled: function BA_browserFramesPrefEnabled() {
    var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
    try {
      return prefs.getBoolPref(BROWSER_FRAMES_ENABLED_PREF);
    }
    catch(e) {
      return false;
    }
  },

  /**
   * Called on browser start, and also when we observe a change in
   * the browser-frames-enabled pref.
   */
  _init: function BA_init() {
    if (this._initialized) {
      return;
    }

    // If browser frames are disabled, watch the pref so we can enable
    // ourselves if the pref is flipped.  This is important for tests, if
    // nothing else.
    if (!this._browserFramesPrefEnabled()) {
      var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
      prefs.addObserver(BROWSER_FRAMES_ENABLED_PREF, this, /* ownsWeak = */ true);
      return;
    }

    this._initialized = true;
    this._progressListener._browserElementAPI = this;

    var os = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
    os.addObserver(this, 'content-document-global-created',  /* ownsWeak = */ true);
    os.addObserver(this, 'docshell-marked-as-browser-frame', /* ownsWeak = */ true);
  },

  /**
   * Called when we observe a docshell-marked-as-browser-frame event, which
   * happens when a docshell is created inside an <iframe mozbrowser>.
   *
   * A docshell may not be un-marked as a browser frame -- this ensures that
   * this event will never fire twice for the same docshell, which guarantees
   * that we'll never register duplicate listeners.
   */
  _observeDocshellMarkedAsBrowserFrame: function BA_observeDocshellMarkedAsBrowserFrame(docshell) {
    docshell.QueryInterface(Ci.nsIWebProgress)
            .addProgressListener(this._progressListener,
                                 Ci.nsIWebProgress.NOTIFY_LOCATION |
                                 Ci.nsIWebProgress.NOTIFY_STATE_WINDOW);
  },

  /**
   * Called when a content window is created.  If the window is directly or
   * indirectly contained in an <iframe mozbrowser>, we'll modify it.
   */
  _observeContentGlobalCreated: function BA_observeContentGlobalCreated(win) {
    var docshell = win.QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsIWebNavigation)
                      .QueryInterface(Ci.nsIDocShell);

    // If this window is not directly or indirectly inside an
    // <iframe mozbrowser>, BrowserElementAPI does nothing to it.
    if (!docshell.containedInBrowserFrame) {
      return;
    }

    this._initBrowserWindow(win, docshell.isBrowserFrame);

    // If this window is directly contained in an <iframe mozbrowser>, do some
    // extra work.
    if (docshell.isBrowserFrame) {
      this._topLevelBrowserWindows.set(win, true);
      this._initTopLevelBrowserWindow(win);
    }
  },

  /**
   * Initialize a content window which is indirectly or directly contained by
   * an <iframe mozbrowser>.
   *
   * |isTopLevel| is true iff |win| is directly contained by an
   * <iframe mozbrowser>.
   */
  _initBrowserWindow: function BA_initBrowserWindow(win, isTopLevel) {
    // XPCNativeWrapper.unwrap gets us the object that content sees; this is
    // the object object that we must define properties on.  Otherwise, the
    // properties will be visible only to chrome!
    var unwrappedWin = XPCNativeWrapper.unwrap(win);

    // This property should exist only on the x-ray wrapped object, not on the
    // unwrapped object, so we define it on |win|, not |unwrappedWin|.
    Object.defineProperty(win, 'browserFrameTop', {
      get: function() {
        if (isTopLevel) {
          return win;
        }

        if ('browserFrameTop' in win.parent) {
          return win.parent.browserFrameTop;
        }

        // This shouldn't happen, but let's at least throw a semi-meaningful
        // error message if it does.
        throw new Error('Internal error in window.browserFrameTop.');
      }
    });

    Object.defineProperty(unwrappedWin, 'top', {
      get: function() {
        return win.browserFrameTop;
      }
    });

    Object.defineProperty(unwrappedWin, 'parent', {
      get: function() {
        if (isTopLevel) {
          return win;
        }
        return win.parent;
      }
    });

    Object.defineProperty(unwrappedWin, 'frameElement', {
      get: function() {
        if (isTopLevel) {
          return null;
        }
        return win.frameElement;
      }
    });
  },

  /**
   * Initialize a content window directly contained by an <iframe mozbrowser>.
   */
  _initTopLevelBrowserWindow: function BA_initTopLevelBrowserWindow(win) {
    // If we haven't seen this window's chrome event handler before, register
    // listeners on it.
    var chromeHandler = win.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIWebNavigation)
                           .QueryInterface(Ci.nsIDocShell)
                           .chromeEventHandler;

    if (chromeHandler && !this._chromeEventHandlersWatching.has(chromeHandler)) {
      this._chromeEventHandlersWatching.set(chromeHandler, true);
      this._addChromeEventHandlerListeners(chromeHandler);
    }
  },

  /**
   * Add some listeners to a chrome event handler.  Don't call this twice for
   * the same chrome event handler or we'll get duplicate listeners!
   */
  _addChromeEventHandlerListeners: function BA_addChromeEventHandlerListeners(chromeHandler) {
    var browserElementAPI = this;

    // Listen for DOMTitleChanged events on top-level <iframe mozbrowser>
    // windows.  (The chrome event handler handles
    chromeHandler.addEventListener(
      'DOMTitleChanged',
      function(e) {
        var win = e.target.defaultView;
        if (browserElementAPI._topLevelBrowserWindows.has(win)) {
          browserElementAPI._fireCustomEvent('titlechange', e.target.title,
                                             win, win.frameElement);
        }
      },
      /* useCapture = */ false,
      /* wantsUntrusted = */ false);
  },

  /**
   * Asynchronously fire a vanilla event at the given window's frame element.
   * (Presumably, the window's frame element is an <iframe mozbrowser>.)
   *
   * We'll prepend 'mozbrowser' to the event's name.
   */
  _fireEvent: function BA_fireEvent(name, win) {
    // Because we're chrome, win.frameElement ignores <iframe mozbrowser>
    // boundaries, as desired.
    var evt = new win.Event('mozbrowser' + name);
    win.setTimeout(function() { win.frameElement.dispatchEvent(evt) }, 0);
  },

  /**
   * Like _fireEvent, but fire a customevent with the given data, instead of a
   * vanilla event.
   */
  _fireCustomEvent: function BA_fireCustomEvent(name, data, win) {
    var evt = new win.CustomEvent('mozbrowser' + name, {detail: data});
    win.setTimeout(function() { win.frameElement.dispatchEvent(evt) }, 0);
  },

  /**
   * An nsIWebProgressListener registered on docshells directly contained in an
   * <iframe mozbrowser>.
   */
  _progressListener: {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                           Ci.nsISupportsWeakReference,
                                           Ci.nsISupports]),

    _getWindow: function(webProgress) {
      return webProgress.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIDOMWindow);
    },

    onLocationChange: function(webProgress, request, location, flags) {
      this._browserElementAPI._fireCustomEvent('locationchange', location.spec,
                                               this._getWindow(webProgress));
    },

    onStateChange: function(webProgress, request, stateFlags, status) {
      if (stateFlags & Ci.nsIWebProgressListener.STATE_START) {
        this._browserElementAPI._fireEvent('loadstart', this._getWindow(webProgress));
      }
      if (stateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
        this._browserElementAPI._fireEvent('loadend', this._getWindow(webProgress));
      }
    },

    onStatusChange: function(webProgress, request, status, message) {},
    onProgressChange: function(webProgress, request, curSelfProgress,
                               maxSelfProgress, curTotalProgress, maxTotalProgress) {},
    onSecurityChange: function(webProgress, request, aState) {}
  },

  /**
   * nsIObserver::Observe
   */
  observe: function BA_observe(subject, topic, data) {
    switch(topic) {
    case 'app-startup':
      this._init();
      break;
    case 'content-document-global-created':
      this._observeContentGlobalCreated(subject);
      break;
    case 'docshell-marked-as-browser-frame':
      this._observeDocshellMarkedAsBrowserFrame(subject);
      break;
    case NS_PREFBRANCH_PREFCHANGE_TOPIC_ID:
      if (data == BROWSER_FRAMES_ENABLED_PREF) {
        this._init();
      }
      break;
    }
  },
};

var NSGetFactory = XPCOMUtils.generateNSGetFactory([BrowserElementAPI]);
