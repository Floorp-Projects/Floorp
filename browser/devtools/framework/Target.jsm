/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "TargetFactory" ];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/commonjs/promise/core.js");
Cu.import("resource:///modules/devtools/EventEmitter.jsm");


const targets = new WeakMap();

/**
 * Functions for creating Targets
 */
this.TargetFactory = {
  /**
   * Construct a Target
   * @param {XULTab} tab
   *        The tab to use in creating a new target
   * @return A target object
   */
  forTab: function TF_forTab(tab) {
    let target = targets.get(tab);
    if (target == null) {
      target = new TabTarget(tab);
      targets.set(tab, target);
    }
    return target;
  },

  /**
   * Creating a target for a tab that is being closed is a problem because it
   * allows a leak as a result of coming after the close event which normally
   * clears things up. This function allows us to ask if there is a known
   * target for a tab without creating a target
   * @return true/false
   */
  isKnownTab: function TF_isKnownTab(tab) {
    return targets.has(tab);
  },

  /**
   * Construct a Target
   * @param {nsIDOMWindow} window
   *        The chromeWindow to use in creating a new target
   * @return A target object
   */
  forWindow: function TF_forWindow(window) {
    let target = targets.get(window);
    if (target == null) {
      target = new WindowTarget(window);
      targets.set(window, target);
    }
    return target;
  },

  /**
   * Construct a Target for a remote global
   * @param {Object} form
   *        The serialized form of a debugging protocol actor.
   * @param {DebuggerClient} client
   *        The debuger client instance to communicate with the server.
   * @param {boolean} chrome
   *        A flag denoting that the debugging target is the remote process as a
   *        whole and not a single tab.
   * @return A target object
   */
  forRemote: function TF_forRemote(form, client, chrome) {
    let target = targets.get(form);
    if (target == null) {
      target = new RemoteTarget(form, client, chrome);
      targets.set(form, target);
    }
    return target;
  },

  /**
   * Get all of the targets known to some browser instance (local if null)
   * @return An array of target objects
   */
  allTargets: function TF_allTargets() {
    let windows = [];
    let wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                       .getService(Components.interfaces.nsIWindowMediator);
    let en = wm.getXULWindowEnumerator(null);
    while (en.hasMoreElements()) {
      windows.push(en.getNext());
    }

    return windows.map(function(window) {
      return TargetFactory.forWindow(window);
    });
  },
};

/**
 * The 'version' property allows the developer tools equivalent of browser
 * detection. Browser detection is evil, however while we don't know what we
 * will need to detect in the future, it is an easy way to postpone work.
 * We should be looking to use 'supports()' in place of version where
 * possible.
 */
function getVersion() {
  // FIXME: return something better
  return 20;
}

/**
 * A better way to support feature detection, but we're not yet at a place
 * where we have the features well enough defined for this to make lots of
 * sense.
 */
function supports(feature) {
  // FIXME: return something better
  return false;
};

/**
 * A Target represents something that we can debug. Targets are generally
 * read-only. Any changes that you wish to make to a target should be done via
 * a Tool that attaches to the target. i.e. a Target is just a pointer saying
 * "the thing to debug is over there".
 *
 * Providing a generalized abstraction of a web-page or web-browser (available
 * either locally or remotely) is beyond the scope of this class (and maybe
 * also beyond the scope of this universe) However Target does attempt to
 * abstract some common events and read-only properties common to many Tools.
 *
 * Supported read-only properties:
 * - name, isRemote, url
 *
 * Target extends EventEmitter and provides support for the following events:
 * - close: The target window has been closed. All tools attached to this
 *     target should close. This event is not currently cancelable.
 * - navigate: The target window has navigated to a different URL
 *
 * Optional events:
 * - will-navigate: The target window will navigate to a different URL
 * - hidden: The target is not visible anymore (for TargetTab, another tab is selected)
 * - visible: The target is visible (for TargetTab, tab is selected)
 *
 * Target also supports 2 functions to help allow 2 different versions of
 * Firefox debug each other. The 'version' property is the equivalent of
 * browser detection - simple and easy to implement but gets fragile when things
 * are not quite what they seem. The 'supports' property is the equivalent of
 * feature detection - harder to setup, but more robust long-term.
 *
 * Comparing Targets: 2 instances of a Target object can point at the same
 * thing, so t1 !== t2 and t1 != t2 even when they represent the same object.
 * To compare to targets use 't1.equals(t2)'.
 */
function Target() {
  throw new Error("Use TargetFactory.newXXX or Target.getXXX to create a Target in place of 'new Target()'");
}

Object.defineProperty(Target.prototype, "version", {
  get: getVersion,
  enumerable: true
});


/**
 * A TabTarget represents a page living in a browser tab. Generally these will
 * be web pages served over http(s), but they don't have to be.
 */
function TabTarget(tab) {
  new EventEmitter(this);
  this._tab = tab;
  this._setupListeners();
}

TabTarget.prototype = {
  _webProgressListener: null,

  supports: supports,
  get version() { return getVersion(); },

  get tab() {
    return this._tab;
  },

  get window() {
    return this._tab.linkedBrowser.contentWindow;
  },

  get name() {
    return this._tab.linkedBrowser.contentDocument.title;
  },

  get url() {
    return this._tab.linkedBrowser.contentDocument.location.href;
  },

  get isRemote() {
    return false;
  },

  get isLocalTab() {
    return true;
  },

  /**
   * Listen to the different tabs events.
   */
  _setupListeners: function TabTarget__setupListeners() {
    this._webProgressListener = new TabWebProgressListener(this);
    this.tab.linkedBrowser.addProgressListener(this._webProgressListener);
    this.tab.addEventListener("TabClose", this);
    this.tab.parentNode.addEventListener("TabSelect", this);
  },

  /**
   * Handle tabs events.
   */
  handleEvent: function (event) {
    switch (event.type) {
      case "TabClose":
        this.destroy();
        break;
      case "TabSelect":
        if (this.tab.selected) {
          this.emit("visible", event);
        } else {
          this.emit("hidden", event);
        }
        break;
    }
  },

  /**
   * Target is not alive anymore.
   */
  destroy: function() {
    if (!this._destroyed) {
      this._destroyed = true;

      this.tab.linkedBrowser.removeProgressListener(this._webProgressListener)
      this._webProgressListener.target = null;
      this._webProgressListener = null;
      this.tab.removeEventListener("TabClose", this);
      this.tab.parentNode.removeEventListener("TabSelect", this);
      this.emit("close");

      targets.delete(this._tab);
      this._tab = null;
    }

    return Promise.resolve(null);
  },

  toString: function() {
    return 'TabTarget:' + this.tab;
  },
};


/**
 * WebProgressListener for TabTarget.
 *
 * @param object aTarget
 *        The TabTarget instance to work with.
 */
function TabWebProgressListener(aTarget) {
  this.target = aTarget;
}

TabWebProgressListener.prototype = {
  target: null,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener, Ci.nsISupportsWeakReference]),

  onStateChange: function TWPL_onStateChange(progress, request, flag, status) {
    let isStart = flag & Ci.nsIWebProgressListener.STATE_START;
    let isDocument = flag & Ci.nsIWebProgressListener.STATE_IS_DOCUMENT;
    let isNetwork = flag & Ci.nsIWebProgressListener.STATE_IS_NETWORK;
    let isRequest = flag & Ci.nsIWebProgressListener.STATE_IS_REQUEST;

    // Skip non-interesting states.
    if (!isStart || !isDocument || !isRequest || !isNetwork) {
      return;
    }

    if (this.target) {
      this.target.emit("will-navigate", request);
    }
  },

  onProgressChange: function() {},
  onSecurityChange: function() {},
  onStatusChange: function() {},

  onLocationChange: function TwPL_onLocationChange(webProgress) {
    let window = webProgress.DOMWindow;
    if (this.target) {
      this.target.emit("navigate", window);
    }
  },
};


/**
 * A WindowTarget represents a page living in a xul window or panel. Generally
 * these will have a chrome: URL
 */
function WindowTarget(window) {
  new EventEmitter(this);
  this._window = window;
}

WindowTarget.prototype = {
  supports: supports,
  get version() { return getVersion(); },

  get window() {
    return this._window;
  },

  get name() {
    return this._window.document.title;
  },

  get url() {
    return this._window.document.location.href;
  },

  get isRemote() {
    return false;
  },

  get isLocalTab() {
    return false;
  },

  /**
   * Target is not alive anymore.
   */
  destroy: function() {
    if (!this._destroyed) {
      this._destroyed = true;

      this.emit("close");

      targets.delete(this._window);
      this._window = null;
    }

    return Promise.resolve(null);
  },

  toString: function() {
    return 'WindowTarget:' + this.window;
  },
};

/**
 * A RemoteTarget represents a page living in a remote Firefox instance.
 */
function RemoteTarget(form, client, chrome) {
  new EventEmitter(this);
  this._client = client;
  this._form = form;
  this._chrome = chrome;

  this.destroy = this.destroy.bind(this);
  this.client.addListener("tabDetached", this.destroy);

  this._onTabNavigated = function onRemoteTabNavigated() {
    this.emit("navigate");
  }.bind(this);
  this.client.addListener("tabNavigated", this._onTabNavigated);
}

RemoteTarget.prototype = {
  supports: supports,
  get version() getVersion(),

  get isRemote() true,

  get chrome() this._chrome,

  get name() this._form._title,

  get url() this._form._url,

  get client() this._client,

  get form() this._form,

  get isLocalTab() false,

  /**
   * Target is not alive anymore.
   */
  destroy: function RT_destroy() {
    // If several things call destroy then we give them all the same
    // destruction promise so we're sure to destroy only once
    if (this._destroyer) {
      return this._destroyer.promise;
    }

    this._destroyer = Promise.defer();

    this.client.removeListener("tabNavigated", this._onTabNavigated);
    this.client.removeListener("tabDetached", this.destroy);

    this._client.close(function onClosed() {
      this._client = null;
      this.emit("close");

      this._destroyer.resolve(null);
    }.bind(this));

    return this._destroyer.promise;
  },

  toString: function() {
    return 'RemoteTarget:' + this.form.actor;
  },
};
