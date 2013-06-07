/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, Cu} = require("chrome");

var Promise = require("sdk/core/promise");
var EventEmitter = require("devtools/shared/event-emitter");

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DebuggerServer",
  "resource://gre/modules/devtools/dbg-server.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DebuggerClient",
  "resource://gre/modules/devtools/dbg-client.jsm");

loader.lazyGetter(this, "InspectorFront", () => require("devtools/server/actors/inspector").InspectorFront);

const targets = new WeakMap();
const promiseTargets = new WeakMap();

/**
 * Functions for creating Targets
 */
exports.TargetFactory = {
  /**
   * Construct a Target
   * @param {XULTab} tab
   *        The tab to use in creating a new target.
   *
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
   * Return a promise of a Target for a remote tab.
   * @param {Object} options
   *        The options object has the following properties:
   *        {
   *          form: the remote protocol form of a tab,
   *          client: a DebuggerClient instance,
   *          chrome: true if the remote target is the whole process
   *        }
   *
   * @return A promise of a target object
   */
  forRemoteTab: function TF_forRemoteTab(options) {
    let promise = promiseTargets.get(options);
    if (promise == null) {
      let target = new TabTarget(options);
      promise = target.makeRemote().then(() => target);
      promiseTargets.set(options, promise);
    }
    return promise;
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
   * Get all of the targets known to the local browser instance
   * @return An array of target objects
   */
  allTargets: function TF_allTargets() {
    let windows = [];
    let wm = Cc["@mozilla.org/appshell/window-mediator;1"]
                       .getService(Ci.nsIWindowMediator);
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
  EventEmitter.decorate(this);
  this.destroy = this.destroy.bind(this);
  this._handleThreadState = this._handleThreadState.bind(this);
  this.on("thread-resumed", this._handleThreadState);
  this.on("thread-paused", this._handleThreadState);
  // Only real tabs need initialization here. Placeholder objects for remote
  // targets will be initialized after a makeRemote method call.
  if (tab && !["client", "form", "chrome"].every(tab.hasOwnProperty, tab)) {
    this._tab = tab;
    this._setupListeners();
  } else {
    this._form = tab.form;
    this._client = tab.client;
    this._chrome = tab.chrome;
  }
}

TabTarget.prototype = {
  _webProgressListener: null,

  supports: supports,
  get version() { return getVersion(); },

  get tab() {
    return this._tab;
  },

  get form() {
    return this._form;
  },

  get root() {
    return this._root;
  },

  get client() {
    return this._client;
  },

  get chrome() {
    return this._chrome;
  },

  get window() {
    // Be extra careful here, since this may be called by HS_getHudByWindow
    // during shutdown.
    if (this._tab && this._tab.linkedBrowser) {
      return this._tab.linkedBrowser.contentWindow;
    }
    return null;
  },

  get name() {
    return this._tab ? this._tab.linkedBrowser.contentDocument.title :
                       this._form.title;
  },

  get url() {
    return this._tab ? this._tab.linkedBrowser.contentDocument.location.href :
                       this._form.url;
  },

  get isRemote() {
    return !this.isLocalTab;
  },

  get isLocalTab() {
    return !!this._tab;
  },

  get isThreadPaused() {
    return !!this._isThreadPaused;
  },

  get inspector() {
    if (!this.form) {
      throw new Error("Target.inspector requires an initialized remote actor.");
    }
    if (this._inspector) {
      return this._inspector;
    }
    this._inspector = InspectorFront(this.client, this.form);
    return this._inspector;
  },

  /**
   * Adds remote protocol capabilities to the target, so that it can be used
   * for tools that support the Remote Debugging Protocol even for local
   * connections.
   */
  makeRemote: function TabTarget_makeRemote() {
    if (this._remote) {
      return this._remote.promise;
    }

    this._remote = Promise.defer();

    if (this.isLocalTab) {
      // Since a remote protocol connection will be made, let's start the
      // DebuggerServer here, once and for all tools.
      if (!DebuggerServer.initialized) {
        DebuggerServer.init();
        DebuggerServer.addBrowserActors();
      }

      this._client = new DebuggerClient(DebuggerServer.connectPipe());
      // A local TabTarget will never perform chrome debugging.
      this._chrome = false;
    }

    this._setupRemoteListeners();

    let attachTab = () => {
      this._client.attachTab(this._form.actor, (aResponse, aTabClient) => {
        if (!aTabClient) {
          this._remote.reject("Unable to attach to the tab");
          return;
        }
        this.threadActor = aResponse.threadActor;
        this._remote.resolve(null);
      });
    };

    if (this.isLocalTab) {
      this._client.connect((aType, aTraits) => {
        this._client.listTabs(aResponse => {
          this._root = aResponse;
          this._form = aResponse.tabs[aResponse.selected];
          attachTab();
        });
      });
    } else if (!this.chrome) {
      // In the remote debugging case, the protocol connection will have been
      // already initialized in the connection screen code.
      attachTab();
    } else {
      // Remote chrome debugging doesn't need anything at this point.
      this._remote.resolve(null);
    }

    return this._remote.promise;
  },

  /**
   * Listen to the different events.
   */
  _setupListeners: function TabTarget__setupListeners() {
    this._webProgressListener = new TabWebProgressListener(this);
    this.tab.linkedBrowser.addProgressListener(this._webProgressListener);
    this.tab.addEventListener("TabClose", this);
    this.tab.parentNode.addEventListener("TabSelect", this);
    this.tab.ownerDocument.defaultView.addEventListener("unload", this);
  },

  /**
   * Setup listeners for remote debugging, updating existing ones as necessary.
   */
  _setupRemoteListeners: function TabTarget__setupRemoteListeners() {
    this.client.addListener("tabDetached", this.destroy);

    this._onTabNavigated = function onRemoteTabNavigated(aType, aPacket) {
      let event = Object.create(null);
      event.url = aPacket.url;
      event.title = aPacket.title;
      event.nativeConsoleAPI = aPacket.nativeConsoleAPI;
      // Send any stored event payload (DOMWindow or nsIRequest) for backwards
      // compatibility with non-remotable tools.
      if (aPacket.state == "start") {
        event._navPayload = this._navRequest;
        this.emit("will-navigate", event);
        this._navRequest = null;
      } else {
        event._navPayload = this._navWindow;
        this.emit("navigate", event);
        this._navWindow = null;
      }
    }.bind(this);
    this.client.addListener("tabNavigated", this._onTabNavigated);
  },

  /**
   * Handle tabs events.
   */
  handleEvent: function (event) {
    switch (event.type) {
      case "TabClose":
      case "unload":
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
   * Handle script status.
   */
  _handleThreadState: function(event) {
    switch (event) {
      case "thread-resumed":
        this._isThreadPaused = false;
        break;
      case "thread-paused":
        this._isThreadPaused = true;
        break;
    }
  },

  /**
   * Target is not alive anymore.
   */
  destroy: function() {
    // If several things call destroy then we give them all the same
    // destruction promise so we're sure to destroy only once
    if (this._destroyer) {
      return this._destroyer.promise;
    }

    this._destroyer = Promise.defer();

    // Before taking any action, notify listeners that destruction is imminent.
    this.emit("close");

    // First of all, do cleanup tasks that pertain to both remoted and
    // non-remoted targets.
    this.off("thread-resumed", this._handleThreadState);
    this.off("thread-paused", this._handleThreadState);

    if (this._tab) {
      if (this._webProgressListener) {
        this._webProgressListener.destroy();
      }

      this._tab.ownerDocument.defaultView.removeEventListener("unload", this);
      this._tab.removeEventListener("TabClose", this);
      this._tab.parentNode.removeEventListener("TabSelect", this);
    }

    // If this target was not remoted, the promise will be resolved before the
    // function returns.
    if (this._tab && !this._client) {
      targets.delete(this._tab);
      this._tab = null;
      this._client = null;
      this._form = null;
      this._remote = null;

      this._destroyer.resolve(null);
    } else if (this._client) {
      // If, on the other hand, this target was remoted, the promise will be
      // resolved after the remote connection is closed.
      this.client.removeListener("tabNavigated", this._onTabNavigated);
      this.client.removeListener("tabDetached", this.destroy);

      this._client.close(function onClosed() {
        if (this._tab) {
          targets.delete(this._tab);
        } else {
          promiseTargets.delete(this._form);
        }
        this._client = null;
        this._tab = null;
        this._form = null;
        this._remote = null;

        this._destroyer.resolve(null);
      }.bind(this));
    }

    return this._destroyer.promise;
  },

  toString: function() {
    return 'TabTarget:' + (this._tab ? this._tab : (this._form && this._form.actor));
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

    // emit event if the top frame is navigating
    if (this.target && this.target.window == progress.DOMWindow) {
      // Emit the event if the target is not remoted or store the payload for
      // later emission otherwise.
      if (this.target._client) {
        this.target._navRequest = request;
      } else {
        this.target.emit("will-navigate", request);
      }
    }
  },

  onProgressChange: function() {},
  onSecurityChange: function() {},
  onStatusChange: function() {},

  onLocationChange: function TWPL_onLocationChange(webProgress, request, URI, flags) {
    if (this.target &&
        !(flags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT)) {
      let window = webProgress.DOMWindow;
      // Emit the event if the target is not remoted or store the payload for
      // later emission otherwise.
      if (this.target._client) {
        this.target._navWindow = window;
      } else {
        this.target.emit("navigate", window);
      }
    }
  },

  /**
   * Destroy the progress listener instance.
   */
  destroy: function TWPL_destroy() {
    if (this.target.tab) {
      this.target.tab.linkedBrowser.removeProgressListener(this);
    }
    this.target._webProgressListener = null;
    this.target._navRequest = null;
    this.target._navWindow = null;
    this.target = null;
  }
};


/**
 * A WindowTarget represents a page living in a xul window or panel. Generally
 * these will have a chrome: URL
 */
function WindowTarget(window) {
  EventEmitter.decorate(this);
  this._window = window;
  this._setupListeners();
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

  get isThreadPaused() {
    return !!this._isThreadPaused;
  },

  /**
   * Listen to the different events.
   */
  _setupListeners: function() {
    this._handleThreadState = this._handleThreadState.bind(this);
    this.on("thread-paused", this._handleThreadState);
    this.on("thread-resumed", this._handleThreadState);
  },

  _handleThreadState: function(event) {
    switch (event) {
      case "thread-resumed":
        this._isThreadPaused = false;
        break;
      case "thread-paused":
        this._isThreadPaused = true;
        break;
    }
  },

  /**
   * Target is not alive anymore.
   */
  destroy: function() {
    if (!this._destroyed) {
      this._destroyed = true;

      this.off("thread-paused", this._handleThreadState);
      this.off("thread-resumed", this._handleThreadState);
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
