/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const defer = require("devtools/shared/defer");
const EventEmitter = require("devtools/shared/event-emitter");
const Services = require("Services");

loader.lazyRequireGetter(this, "DebuggerServer", "devtools/server/main", true);
loader.lazyRequireGetter(this, "DebuggerClient",
  "devtools/shared/client/debugger-client", true);
loader.lazyRequireGetter(this, "gDevTools",
  "devtools/client/framework/devtools", true);

const targets = new WeakMap();
const promiseTargets = new WeakMap();

/**
 * Functions for creating Targets
 */
const TargetFactory = exports.TargetFactory = {
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
      const target = new TabTarget(options);
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
  // Default isBrowsingContext to true if not explicitly specified
  if (typeof tab.isBrowsingContext == "boolean") {
    this._isBrowsingContext = tab.isBrowsingContext;
  } else {
    this._isBrowsingContext = true;
  }
}

exports.TabTarget = TabTarget;

TabTarget.prototype = {
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

    const deferred = defer();

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

  // Get a promise of the root form returned by a getRoot request. This promise
  // is cached.
  get root() {
    if (!this._root) {
      this._root = this._getRoot();
    }
    return this._root;
  },

  _getRoot: function() {
    return new Promise((resolve, reject) => {
      this.client.mainRoot.getRoot(response => {
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

  // Tells us if the related actor implements BrowsingContextTargetActor
  // interface and requires to call `attach` request before being used and
  // `detach` during cleanup.
  // TODO: This flag is quite confusing, try to find a better way.
  // Bug 1465635 hopes to blow up these classes entirely.
  get isBrowsingContext() {
    return this._isBrowsingContext;
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
    const isLegacyAddon = !!(this._form && this._form.actor &&
      this._form.actor.match(/conn\d+\.addon(Target)?\d+/));
    return isLegacyAddon || this.isWebExtension;
  },

  get isWebExtension() {
    return !!(this._form && this._form.actor && (
      this._form.actor.match(/conn\d+\.webExtension(Target)?\d+/) ||
      this._form.actor.match(/child\d+\/webExtension(Target)?\d+/)
    ));
  },

  get isLocalTab() {
    return !!this._tab;
  },

  get isMultiProcess() {
    return !this.window;
  },

  getExtensionPathName(url) {
    // Return the url if the target is not a webextension.
    if (!this.isWebExtension) {
      throw new Error("Target is not a WebExtension");
    }

    try {
      const parsedURL = new URL(url);
      // Only moz-extension URL should be shortened into the URL pathname.
      if (parsedURL.protocol !== "moz-extension:") {
        return url;
      }
      return parsedURL.pathname;
    } catch (e) {
      // Return the url if unable to resolve the pathname.
      return url;
    }
  },

  /**
   * For local tabs, returns the tab's contentPrincipal, which can be used as a
   * `triggeringPrincipal` when opening links.  However, this is a hack as it is not
   * correct for subdocuments and it won't work for remote debugging.  Bug 1467945 hopes
   * to devise a better approach.
   */
  get contentPrincipal() {
    if (!this.isLocalTab) {
      return null;
    }
    return this.tab.linkedBrowser.contentPrincipal;
  },

  /**
   * Adds remote protocol capabilities to the target, so that it can be used
   * for tools that support the Remote Debugging Protocol even for local
   * connections.
   */
  makeRemote: async function() {
    if (this._remote) {
      return this._remote.promise;
    }

    this._remote = defer();

    if (this.isLocalTab) {
      // Since a remote protocol connection will be made, let's start the
      // DebuggerServer here, once and for all tools.
      DebuggerServer.init();

      // When connecting to a local tab, we only need the root actor.
      // Then we are going to call DebuggerServer.connectToFrame and talk
      // directly with actors living in the child process.
      // We also need browser actors for actor registry which enabled addons
      // to register custom actors.
      // TODO: the comment and implementation are out of sync here. See Bug 1420134.
      DebuggerServer.registerAllActors();
      // Enable being able to get child process actors
      DebuggerServer.allowChromeProcess = true;

      this._client = new DebuggerClient(DebuggerServer.connectPipe());
      // A local TabTarget will never perform chrome debugging.
      this._chrome = false;
    } else if (this._form.isWebExtension &&
          this.client.mainRoot.traits.webExtensionAddonConnect) {
      // The addonTargetActor form is related to a WebExtensionActor instance,
      // which isn't a target actor on its own, it is an actor living in the parent
      // process with access to the addon metadata, it can control the addon (e.g.
      // reloading it) and listen to the AddonManager events related to the lifecycle of
      // the addon (e.g. when the addon is disabled or uninstalled).
      // To retrieve the target actor instance, we call its "connect" method, (which
      // fetches the target actor form from a WebExtensionTargetActor instance).
      const {form} = await this._client.request({
        to: this._form.actor, type: "connect",
      });

      this._form = form;
      this._url = form.url;
      this._title = form.title;
    }

    this._setupRemoteListeners();

    const attachTab = async () => {
      try {
        const [ response, tabClient ] = await this._client.attachTab(this._form.actor);
        this.activeTab = tabClient;
        this.threadActor = response.threadActor;
      } catch (e) {
        this._remote.reject("Unable to attach to the tab: " + e);
        return;
      }
      attachConsole();
    };

    const onConsoleAttached = ([response, consoleClient]) => {
      this.activeConsole = consoleClient;

      this._onInspectObject = packet => this.emit("inspect-object", packet);
      this.activeConsole.on("inspectObject", this._onInspectObject);

      this._remote.resolve(null);
    };

    const attachConsole = () => {
      this._client.attachConsole(this._form.consoleActor, [])
        .then(onConsoleAttached, response => {
          this._remote.reject(
            `Unable to attach to the console [${response.error}]: ${response.message}`);
        });
    };

    if (this.isLocalTab) {
      this._client.connect()
        .then(() => this._client.getTab({ tab: this.tab }))
        .then(response => {
          this._form = response.tab;
          this._url = this._form.url;
          this._title = this._form.title;

          attachTab();
        }, e => this._remote.reject(e));
    } else if (this.isBrowsingContext) {
      // In the remote debugging case, the protocol connection will have been
      // already initialized in the connection screen code.
      attachTab();
    } else {
      // AddonActor and chrome debugging on RootActor doesn't inherit from
      // BrowsingContextTargetActor and doesn't need to be attached.
      attachConsole();
    }

    return this._remote.promise;
  },

  /**
   * Listen to the different events.
   */
  _setupListeners: function() {
    this.tab.addEventListener("TabClose", this);
    this.tab.parentNode.addEventListener("TabSelect", this);
    this.tab.ownerDocument.defaultView.addEventListener("unload", this);
    this.tab.addEventListener("TabRemotenessChange", this);
  },

  /**
   * Teardown event listeners.
   */
  _teardownListeners: function() {
    this._tab.ownerDocument.defaultView.removeEventListener("unload", this);
    this._tab.removeEventListener("TabClose", this);
    this._tab.parentNode.removeEventListener("TabSelect", this);
    this._tab.removeEventListener("TabRemotenessChange", this);
  },

  /**
   * Setup listeners for remote debugging, updating existing ones as necessary.
   */
  _setupRemoteListeners: function() {
    this.client.addListener("closed", this.destroy);

    this._onTabDetached = (type, packet) => {
      // We have to filter message to ensure that this detach is for this tab
      if (packet.from == this._form.actor) {
        this.destroy();
      }
    };
    this.client.addListener("tabDetached", this._onTabDetached);

    this._onTabNavigated = (type, packet) => {
      const event = Object.create(null);
      event.url = packet.url;
      event.title = packet.title;
      event.nativeConsoleAPI = packet.nativeConsoleAPI;
      event.isFrameSwitching = packet.isFrameSwitching;

      // Keep the title unmodified when a developer toolbox switches frame
      // for a tab (Bug 1261687), but always update the title when the target
      // is a WebExtension (where the addon name is always included in the title
      // and the url is supposed to be updated every time the selected frame changes).
      if (!packet.isFrameSwitching || this.isWebExtension) {
        this._url = packet.url;
        this._title = packet.title;
      }

      // Send any stored event payload (DOMWindow or nsIRequest) for backwards
      // compatibility with non-remotable tools.
      if (packet.state == "start") {
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

    this._onFrameUpdate = (type, packet) => {
      this.emit("frame-update", packet);
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
    if (this.activeConsole && this._onInspectObject) {
      this.activeConsole.off("inspectObject", this._onInspectObject);
    }
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
      case "TabRemotenessChange":
        this.onRemotenessChange();
        break;
    }
  },

  /**
   * Automatically respawn the toolbox when the tab changes between being
   * loaded within the parent process and loaded from a content process.
   * Process change can go in both ways.
   */
  onRemotenessChange: function() {
    // Responsive design do a crazy dance around tabs and triggers
    // remotenesschange events. But we should ignore them as at the end
    // the content doesn't change its remoteness.
    if (this._tab.isResponsiveDesignMode) {
      return;
    }

    // Save a reference to the tab as it will be nullified on destroy
    const tab = this._tab;
    const onToolboxDestroyed = target => {
      if (target != this) {
        return;
      }
      gDevTools.off("toolbox-destroyed", target);

      // Recreate a fresh target instance as the current one is now destroyed
      const newTarget = TargetFactory.forTab(tab);
      gDevTools.showToolbox(newTarget);
    };
    gDevTools.on("toolbox-destroyed", onToolboxDestroyed);
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

    this._destroyer = defer();

    // Before taking any action, notify listeners that destruction is imminent.
    this.emit("close");

    if (this._tab) {
      this._teardownListeners();
    }

    const cleanupAndResolve = () => {
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
        this._client.close().then(cleanupAndResolve);
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
    this._title = null;
    this._url = null;
    this.threadActor = null;
  },

  toString: function() {
    const id = this._tab ? this._tab : (this._form && this._form.actor);
    return `TabTarget:${id}`;
  },

  /**
   * Log an error of some kind to the tab's console.
   *
   * @param {String} text
   *                 The text to log.
   * @param {String} category
   *                 The category of the message.  @see nsIScriptError.
   */
  logErrorInPage: function(text, category) {
    if (this.activeTab && this.activeTab.traits.logInPage) {
      const errorFlag = 0;
      const packet = {
        to: this.form.actor,
        type: "logInPage",
        flags: errorFlag,
        text,
        category,
      };
      this.client.request(packet);
    }
  },

  /**
   * Log a warning of some kind to the tab's console.
   *
   * @param {String} text
   *                 The text to log.
   * @param {String} category
   *                 The category of the message.  @see nsIScriptError.
   */
  logWarningInPage: function(text, category) {
    if (this.activeTab && this.activeTab.traits.logInPage) {
      const warningFlag = 1;
      const packet = {
        to: this.form.actor,
        type: "logInPage",
        flags: warningFlag,
        text,
        category,
      };
      this.client.request(packet);
    }
  },
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

  get isBrowsingContext() {
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

  get activeConsole() {
    return this.client._clients.get(this.form.consoleActor);
  },

  get client() {
    return this._workerClient.client;
  },

  destroy: function() {
    this._workerClient.detach();
  },

  hasActor: function(name) {
    // console is the only one actor implemented by WorkerTargetActor
    if (name == "console") {
      return true;
    }
    return false;
  },

  getTrait: function() {
    return undefined;
  },

  makeRemote: function() {
    return Promise.resolve();
  },

  logErrorInPage: function() {
    // No-op.  See bug 1368680.
  },

  logWarningInPage: function() {
    // No-op.  See bug 1368680.
  },
};
