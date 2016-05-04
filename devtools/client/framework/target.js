/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci, Cu } = require("chrome");
const promise = require("promise");
const EventEmitter = require("devtools/shared/event-emitter");
const Services = require("Services");

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
loader.lazyRequireGetter(this, "DebuggerServer", "devtools/server/main", true);
loader.lazyRequireGetter(this, "DebuggerClient",
  "devtools/shared/client/main", true);

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
  forTab: function(tab) {
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
   *          client: a DebuggerClient instance
   *                  (caller owns this and is responsible for closing),
   *          chrome: true if the remote target is the whole process
   *        }
   *
   * @return A promise of a target object
   */
  forRemoteTab: function(options) {
    let targetPromise = promiseTargets.get(options);
    if (targetPromise == null) {
      let target = new TabTarget(options);
      targetPromise = target.makeRemote().then(() => target);
      promiseTargets.set(options, targetPromise);
    }
    return targetPromise;
  },

  forWorker: function(workerClient) {
    let target = targets.get(workerClient);
    if (target == null) {
      target = new WorkerTarget(workerClient);
      targets.set(workerClient, target);
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
  isKnownTab: function(tab) {
    return targets.has(tab);
  },
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
 *          target should close. This event is not currently cancelable.
 * - navigate: The target window has navigated to a different URL
 *
 * Optional events:
 * - will-navigate: The target window will navigate to a different URL
 * - hidden: The target is not visible anymore (for TargetTab, another tab is
 *           selected)
 * - visible: The target is visible (for TargetTab, tab is selected)
 *
 * Comparing Targets: 2 instances of a Target object can point at the same
 * thing, so t1 !== t2 and t1 != t2 even when they represent the same object.
 * To compare to targets use 't1.equals(t2)'.
 */

/**
 * A TabTarget represents a page living in a browser tab. Generally these will
 * be web pages served over http(s), but they don't have to be.
 */
function TabTarget(tab) {
  EventEmitter.decorate(this);
  this.destroy = this.destroy.bind(this);
  this.activeTab = this.activeConsole = null;
  // Only real tabs need initialization here. Placeholder objects for remote
  // targets will be initialized after a makeRemote method call.
  if (tab && !["client", "form", "chrome"].every(tab.hasOwnProperty, tab)) {
    this._tab = tab;
    this._setupListeners();
  } else {
    this._form = tab.form;
    this._url = this._form.url;
    this._title = this._form.title;

    this._client = tab.client;
    this._chrome = tab.chrome;
  }
  // Default isTabActor to true if not explicitly specified
  if (typeof tab.isTabActor == "boolean") {
    this._isTabActor = tab.isTabActor;
  } else {
    this._isTabActor = true;
  }
}

TabTarget.prototype = {
  _webProgressListener: null,

  /**
   * Returns a promise for the protocol description from the root actor. Used
   * internally with `target.actorHasMethod`. Takes advantage of caching if
   * definition was fetched previously with the corresponding actor information.
   * Actors are lazily loaded, so not only must the tool using a specific actor
   * be in use, the actors are only registered after invoking a method (for
   * performance reasons, added in bug 988237), so to use these actor detection
   * methods, one must already be communicating with a specific actor of that
   * type.
   *
   * Must be a remote target.
   *
   * @return {Promise}
   * {
   *   "category": "actor",
   *   "typeName": "longstractor",
   *   "methods": [{
   *     "name": "substring",
   *     "request": {
   *       "type": "substring",
   *       "start": {
   *         "_arg": 0,
   *         "type": "primitive"
   *       },
   *       "end": {
   *         "_arg": 1,
   *         "type": "primitive"
   *       }
   *     },
   *     "response": {
   *       "substring": {
   *         "_retval": "primitive"
   *       }
   *     }
   *   }],
   *  "events": {}
   * }
   */
  getActorDescription: function(actorName) {
    if (!this.client) {
      throw new Error("TabTarget#getActorDescription() can only be called on " +
                      "remote tabs.");
    }

    let deferred = promise.defer();

    if (this._protocolDescription &&
        this._protocolDescription.types[actorName]) {
      deferred.resolve(this._protocolDescription.types[actorName]);
    } else {
      this.client.mainRoot.protocolDescription(description => {
        this._protocolDescription = description;
        deferred.resolve(description.types[actorName]);
      });
    }

    return deferred.promise;
  },

  /**
   * Returns a boolean indicating whether or not the specific actor
   * type exists. Must be a remote target.
   *
   * @param {String} actorName
   * @return {Boolean}
   */
  hasActor: function(actorName) {
    if (!this.client) {
      throw new Error("TabTarget#hasActor() can only be called on remote " +
                      "tabs.");
    }
    if (this.form) {
      return !!this.form[actorName + "Actor"];
    }
    return false;
  },

  /**
   * Queries the protocol description to see if an actor has
   * an available method. The actor must already be lazily-loaded (read
   * the restrictions in the `getActorDescription` comments),
   * so this is for use inside of tool. Returns a promise that
   * resolves to a boolean. Must be a remote target.
   *
   * @param {String} actorName
   * @param {String} methodName
   * @return {Promise}
   */
  actorHasMethod: function(actorName, methodName) {
    if (!this.client) {
      throw new Error("TabTarget#actorHasMethod() can only be called on " +
                      "remote tabs.");
    }
    return this.getActorDescription(actorName).then(desc => {
      if (desc && desc.methods) {
        return !!desc.methods.find(method => method.name === methodName);
      }
      return false;
    });
  },

  /**
   * Returns a trait from the root actor.
   *
   * @param {String} traitName
   * @return {Mixed}
   */
  getTrait: function(traitName) {
    if (!this.client) {
      throw new Error("TabTarget#getTrait() can only be called on remote " +
                      "tabs.");
    }

    // If the targeted actor exposes traits and has a defined value for this
    // traits, override the root actor traits
    if (this.form.traits && traitName in this.form.traits) {
      return this.form.traits[traitName];
    }

    return this.client.traits[traitName];
  },

  get tab() {
    return this._tab;
  },

  get form() {
    return this._form;
  },

  // Get a promise of the root form returned by a listTabs request. This promise
  // is cached.
  get root() {
    if (!this._root) {
      this._root = this._getRoot();
    }
    return this._root;
  },

  _getRoot: function() {
    return new Promise((resolve, reject) => {
      this.client.listTabs(response => {
        if (response.error) {
          reject(new Error(response.error + ": " + response.message));
          return;
        }

        resolve(response);
      });
    });
  },

  get client() {
    return this._client;
  },

  // Tells us if we are debugging content document
  // or if we are debugging chrome stuff.
  // Allows to controls which features are available against
  // a chrome or a content document.
  get chrome() {
    return this._chrome;
  },

  // Tells us if the related actor implements TabActor interface
  // and requires to call `attach` request before being used
  // and `detach` during cleanup
  get isTabActor() {
    return this._isTabActor;
  },

  get window() {
    // XXX - this is a footgun for e10s - there .contentWindow will be null,
    // and even though .contentWindowAsCPOW *might* work, it will not work
    // in all contexts.  Consumers of .window need to be refactored to not
    // rely on this.
    if (Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT) {
      console.error("The .window getter on devtools' |target| object isn't " +
                    "e10s friendly!\n" + Error().stack);
    }
    // Be extra careful here, since this may be called by HS_getHudByWindow
    // during shutdown.
    if (this._tab && this._tab.linkedBrowser) {
      return this._tab.linkedBrowser.contentWindow;
    }
    return null;
  },

  get name() {
    if (this.isAddon) {
      return this._form.name;
    }
    return this._title;
  },

  get url() {
    return this._url;
  },

  get isRemote() {
    return !this.isLocalTab;
  },

  get isAddon() {
    return !!(this._form && this._form.actor &&
              this._form.actor.match(/conn\d+\.addon\d+/));
  },

  get isLocalTab() {
    return !!this._tab;
  },

  get isMultiProcess() {
    return !this.window;
  },

  /**
   * Adds remote protocol capabilities to the target, so that it can be used
   * for tools that support the Remote Debugging Protocol even for local
   * connections.
   */
  makeRemote: function() {
    if (this._remote) {
      return this._remote.promise;
    }

    this._remote = promise.defer();

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
      this._client.attachTab(this._form.actor, (response, tabClient) => {
        if (!tabClient) {
          this._remote.reject("Unable to attach to the tab");
          return;
        }
        this.activeTab = tabClient;
        this.threadActor = response.threadActor;

        attachConsole();
      });
    };

    let onConsoleAttached = (response, consoleClient) => {
      if (!consoleClient) {
        this._remote.reject("Unable to attach to the console");
        return;
      }
      this.activeConsole = consoleClient;
      this._remote.resolve(null);
    };

    let attachConsole = () => {
      this._client.attachConsole(this._form.consoleActor,
                                 [ "NetworkActivity" ],
                                 onConsoleAttached);
    };

    if (this.isLocalTab) {
      this._client.connect()
        .then(() => this._client.getTab({ tab: this.tab }))
        .then(response => {
          this._form = response.tab;
          this._url = this._form.url;
          this._title = this._form.title;

          attachTab();
        });
    } else if (this.isTabActor) {
      // In the remote debugging case, the protocol connection will have been
      // already initialized in the connection screen code.
      attachTab();
    } else {
      // AddonActor and chrome debugging on RootActor doesn't inherits from
      // TabActor and doesn't need to be attached.
      attachConsole();
    }

    return this._remote.promise;
  },

  /**
   * Listen to the different events.
   */
  _setupListeners: function() {
    this._webProgressListener = new TabWebProgressListener(this);
    this.tab.linkedBrowser.addProgressListener(this._webProgressListener);
    this.tab.addEventListener("TabClose", this);
    this.tab.parentNode.addEventListener("TabSelect", this);
    this.tab.ownerDocument.defaultView.addEventListener("unload", this);
  },

  /**
   * Teardown event listeners.
   */
  _teardownListeners: function() {
    if (this._webProgressListener) {
      this._webProgressListener.destroy();
    }

    this._tab.ownerDocument.defaultView.removeEventListener("unload", this);
    this._tab.removeEventListener("TabClose", this);
    this._tab.parentNode.removeEventListener("TabSelect", this);
  },

  /**
   * Setup listeners for remote debugging, updating existing ones as necessary.
   */
  _setupRemoteListeners: function() {
    this.client.addListener("closed", this.destroy);

    this._onTabDetached = (aType, aPacket) => {
      // We have to filter message to ensure that this detach is for this tab
      if (aPacket.from == this._form.actor) {
        this.destroy();
      }
    };
    this.client.addListener("tabDetached", this._onTabDetached);

    this._onTabNavigated = (aType, aPacket) => {
      let event = Object.create(null);
      event.url = aPacket.url;
      event.title = aPacket.title;
      event.nativeConsoleAPI = aPacket.nativeConsoleAPI;
      event.isFrameSwitching = aPacket.isFrameSwitching;

      if (!aPacket.isFrameSwitching) {
        // Update the title and url unless this is a frame switch.
        this._url = aPacket.url;
        this._title = aPacket.title;
      }

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
    };
    this.client.addListener("tabNavigated", this._onTabNavigated);

    this._onFrameUpdate = (aType, aPacket) => {
      this.emit("frame-update", aPacket);
    };
    this.client.addListener("frameUpdate", this._onFrameUpdate);

    this._onSourceUpdated = (event, packet) => this.emit("source-updated", packet);
    this.client.addListener("newSource", this._onSourceUpdated);
    this.client.addListener("updatedSource", this._onSourceUpdated);
  },

  /**
   * Teardown listeners for remote debugging.
   */
  _teardownRemoteListeners: function() {
    this.client.removeListener("closed", this.destroy);
    this.client.removeListener("tabNavigated", this._onTabNavigated);
    this.client.removeListener("tabDetached", this._onTabDetached);
    this.client.removeListener("frameUpdate", this._onFrameUpdate);
    this.client.removeListener("newSource", this._onSourceUpdated);
    this.client.removeListener("updatedSource", this._onSourceUpdated);
  },

  /**
   * Handle tabs events.
   */
  handleEvent: function(event) {
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
   * Target is not alive anymore.
   */
  destroy: function() {
    // If several things call destroy then we give them all the same
    // destruction promise so we're sure to destroy only once
    if (this._destroyer) {
      return this._destroyer.promise;
    }

    this._destroyer = promise.defer();

    // Before taking any action, notify listeners that destruction is imminent.
    this.emit("close");

    if (this._tab) {
      this._teardownListeners();
    }

    let cleanupAndResolve = () => {
      this._cleanup();
      this._destroyer.resolve(null);
    };
    // If this target was not remoted, the promise will be resolved before the
    // function returns.
    if (this._tab && !this._client) {
      cleanupAndResolve();
    } else if (this._client) {
      // If, on the other hand, this target was remoted, the promise will be
      // resolved after the remote connection is closed.
      this._teardownRemoteListeners();

      if (this.isLocalTab) {
        // We started with a local tab and created the client ourselves, so we
        // should close it.
        this._client.close(cleanupAndResolve);
      } else if (this.activeTab) {
        // The client was handed to us, so we are not responsible for closing
        // it. We just need to detach from the tab, if already attached.
        // |detach| may fail if the connection is already dead, so proceed with
        // cleanup directly after this.
        this.activeTab.detach();
        cleanupAndResolve();
      } else {
        cleanupAndResolve();
      }
    }

    return this._destroyer.promise;
  },

  /**
   * Clean up references to what this target points to.
   */
  _cleanup: function() {
    if (this._tab) {
      targets.delete(this._tab);
    } else {
      promiseTargets.delete(this._form);
    }

    this.activeTab = null;
    this.activeConsole = null;
    this._client = null;
    this._tab = null;
    this._form = null;
    this._remote = null;
    this._root = null;
  },

  toString: function() {
    let id = this._tab ? this._tab : (this._form && this._form.actor);
    return `TabTarget:${id}`;
  },

  /**
   * @see TabActor.prototype.onResolveLocation
   */
  resolveLocation(loc) {
    let deferred = promise.defer();

    this.client.request(Object.assign({
      to: this._form.actor,
      type: "resolveLocation",
    }, loc), deferred.resolve);

    return deferred.promise;
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

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference]),

  onStateChange: function(progress, request, flag) {
    let isStart = flag & Ci.nsIWebProgressListener.STATE_START;
    let isDocument = flag & Ci.nsIWebProgressListener.STATE_IS_DOCUMENT;
    let isNetwork = flag & Ci.nsIWebProgressListener.STATE_IS_NETWORK;
    let isRequest = flag & Ci.nsIWebProgressListener.STATE_IS_REQUEST;

    // Skip non-interesting states.
    if (!isStart || !isDocument || !isRequest || !isNetwork) {
      return;
    }

    // emit event if the top frame is navigating
    if (progress.isTopLevel) {
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

  onLocationChange: function(webProgress, request, URI, flags) {
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
  destroy: function() {
    if (this.target.tab) {
      try {
        this.target.tab.linkedBrowser.removeProgressListener(this);
      } catch (ex) {
        // This can throw when a tab crashes in e10s.
      }
    }
    this.target._webProgressListener = null;
    this.target._navRequest = null;
    this.target._navWindow = null;
    this.target = null;
  }
};

function WorkerTarget(workerClient) {
  EventEmitter.decorate(this);
  this._workerClient = workerClient;
}

/**
 * A WorkerTarget represents a worker. Unlike TabTarget, which can represent
 * either a local or remote tab, WorkerTarget always represents a remote worker.
 * Moreover, unlike TabTarget, which is constructed with a placeholder object
 * for remote tabs (from which a TabClient can then be lazily obtained),
 * WorkerTarget is constructed with a WorkerClient directly.
 *
 * WorkerClient is designed to mimic the interface of TabClient as closely as
 * possible. This allows us to debug workers as if they were ordinary tabs,
 * requiring only minimal changes to the rest of the frontend.
 */
WorkerTarget.prototype = {
  get isRemote() {
    return true;
  },

  get isTabActor() {
    return true;
  },

  get name() {
    return "Worker";
  },

  get url() {
    return this._workerClient.url;
  },

  get isWorkerTarget() {
    return true;
  },

  get form() {
    return {
      consoleActor: this._workerClient.consoleActor
    };
  },

  get activeTab() {
    return this._workerClient;
  },

  get client() {
    return this._workerClient.client;
  },

  destroy: function() {},

  hasActor: function (name) {
    // console is the only one actor implemented by WorkerActor
    if (name == "console") {
      return true;
    }
    return false;
  },

  getTrait: function() {
    return undefined;
  },

  makeRemote: function () {
    return Promise.resolve();
  }
};
