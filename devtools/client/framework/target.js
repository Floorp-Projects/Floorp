/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const Services = require("Services");

loader.lazyRequireGetter(this, "DebuggerServer", "devtools/server/main", true);
loader.lazyRequireGetter(this, "DebuggerClient",
  "devtools/shared/client/debugger-client", true);
loader.lazyRequireGetter(this, "gDevTools",
  "devtools/client/framework/devtools", true);
loader.lazyRequireGetter(this, "getFront", "devtools/shared/protocol", true);

const targets = new WeakMap();
const promiseTargets = new WeakMap();

/**
 * Functions for creating Targets
 */
const TargetFactory = exports.TargetFactory = {

  /**
   * Construct a Target. The target will be cached for each Tab so that we create only
   * one per tab.
   *
   * @param {XULTab} tab
   *        The tab to use in creating a new target.
   *
   * @return A target object
   */
  forTab: async function(tab) {
    let target = targets.get(tab);
    if (target) {
      return target;
    }
    const promise = this.createTargetForTab(tab);
    // Immediately set the target's promise in cache to prevent race
    targets.set(tab, promise);
    target = await promise;
    // Then replace the promise with the target object
    targets.set(tab, target);
    return target;
  },

  /**
   * Instantiate a target for the given tab.
   *
   * This will automatically:
   * - spawn a DebuggerServer in the parent process,
   * - create a DebuggerClient and connect it to this local DebuggerServer,
   * - call RootActor's `getTab` request to retrieve the FrameTargetActor's form,
   * - instantiate a Target instance.
   *
   * @param {XULTab} tab
   *        The tab to use in creating a new target.
   *
   * @return A target object
   */
  async createTargetForTab(tab) {
    function createLocalServer() {
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
    }

    function createLocalClient() {
      return new DebuggerClient(DebuggerServer.connectPipe());
    }

    createLocalServer();
    const client = createLocalClient();

    // Connect the local client to the local server
    await client.connect();

    // Fetch the FrameTargetActor's Front
    const front = await client.mainRoot.getTab({ tab });

    return new Target({
      client,
      activeTab: front,
      // A local Target will never perform chrome debugging.
      chrome: false,
      tab,
    });
  },

  /**
   * Return a promise of a Target for a remote tab.
   * @param {Object} options
   *        The options object has the following properties:
   *        {
   *          activeTab: front for this tab target,
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
      const target = new Target(options);
      targetPromise = target.attach().then(() => target);
      targetPromise.catch(e => {
        console.error("Exception while attaching target", e);
      });
      promiseTargets.set(options, targetPromise);
    }
    return targetPromise;
  },

  forWorker: function(workerTargetFront) {
    let target = targets.get(workerTargetFront);
    if (target == null) {
      target = new Target({
        client: workerTargetFront.client,
        activeTab: workerTargetFront,
        chrome: false,
      });
      targets.set(workerTargetFront, target);
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
 * - name, url
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
 * A Target represents a debuggable context. It can be a browser tab, a tab on
 * a remote device, like a tab on Firefox for Android. But it can also be an add-on,
 * as well as firefox parent process, or just one of its content process.
 * A Target is related to a given TargetActor, for which we pass the form as
 * argument.
 *
 * For now, only workers are having a distinct Target class called WorkerTarget.
 *
 * @param {Front} activeTab
 *                  If we already have a front for this target, pass it here.
 * @param {DebuggerClient} client
 *                  The DebuggerClient instance to be used to debug this target.
 * @param {Boolean} chrome
 *                  True, if we allow to see privileged resources like JSM, xpcom,
 *                  frame scripts...
 * @param {xul:tab} tab (optional)
 *                  If the target is a local Firefox tab, a reference to the firefox
 *                  frontend tab object.
 */
class Target extends EventEmitter {
  constructor({ client, chrome, activeTab, tab = null }) {
    if (!activeTab) {
      throw new Error("Cannot instanciate target without a non-null activeTab");
    }

    super();

    this.destroy = this.destroy.bind(this);
    this._onTabNavigated = this._onTabNavigated.bind(this);
    this.activeConsole = null;
    this.activeTab = activeTab;

    this._url = this.form.url;
    this._title = this.form.title;

    this._client = client;
    this._chrome = chrome;

    // When debugging local tabs, we also have a reference to the Firefox tab
    // This is used to:
    // * distinguish local tabs from remote (see target.isLocalTab)
    // * being able to hookup into Firefox UI (see Hosts)
    if (tab) {
      this._tab = tab;
      this._setupListeners();
    }

    // isBrowsingContext is true for all target connected to an actor that inherits from
    // BrowsingContextTargetActor. It happens to be the case for almost all targets but:
    // * legacy add-ons (old bootstrapped add-ons)
    // * content process (browser content toolbox)
    // * xpcshell debugging (it uses ParentProcessTargetActor, which inherits from
    //                       BrowsingContextActor, but doesn't have any valid browsing
    //                       context to attach to.)
    // Starting with FF64, BrowsingContextTargetActor exposes a traits to help identify
    // the target actors inheriting from it. It also help identify the xpcshell debugging
    // target actor that doesn't have any valid browsing context.
    // (Once FF63 is no longer supported, we can remove the `else` branch and only look
    // for the traits)
    if (this.form.traits && ("isBrowsingContext" in this.form.traits)) {
      this._isBrowsingContext = this.form.traits.isBrowsingContext;
    } else {
      this._isBrowsingContext = !this.isLegacyAddon && !this.isContentProcess && !this.isWorkerTarget;
    }

    // Cache of already created targed-scoped fronts
    // [typeName:string => Front instance]
    this.fronts = new Map();
    // Temporary fix for bug #1493131 - inspector has a different life cycle
    // than most other fronts because it is closely related to the toolbox.
    // TODO: remove once inspector is separated from the toolbox
    this._inspector = null;
  }

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
  async getActorDescription(actorName) {
    if (this._protocolDescription &&
        this._protocolDescription.types[actorName]) {
      return this._protocolDescription.types[actorName];
    }
    const description = await this.client.mainRoot.protocolDescription();
    this._protocolDescription = description;
    return description.types[actorName];
  }

  /**
   * Returns a boolean indicating whether or not the specific actor
   * type exists.
   *
   * @param {String} actorName
   * @return {Boolean}
   */
  hasActor(actorName) {
    if (this.form) {
      return !!this.form[actorName + "Actor"];
    }
    return false;
  }

  /**
   * Queries the protocol description to see if an actor has
   * an available method. The actor must already be lazily-loaded (read
   * the restrictions in the `getActorDescription` comments),
   * so this is for use inside of tool. Returns a promise that
   * resolves to a boolean.
   *
   * @param {String} actorName
   * @param {String} methodName
   * @return {Promise}
   */
  actorHasMethod(actorName, methodName) {
    return this.getActorDescription(actorName).then(desc => {
      if (desc && desc.methods) {
        return !!desc.methods.find(method => method.name === methodName);
      }
      return false;
    });
  }

  /**
   * Returns a trait from the root actor.
   *
   * @param {String} traitName
   * @return {Mixed}
   */
  getTrait(traitName) {
    // If the targeted actor exposes traits and has a defined value for this
    // traits, override the root actor traits
    if (this.form.traits && traitName in this.form.traits) {
      return this.form.traits[traitName];
    }

    return this.client.traits[traitName];
  }

  get tab() {
    return this._tab;
  }

  get form() {
    return this.activeTab.targetForm;
  }

  // Get a promise of the RootActor's form
  get root() {
    return this.client.mainRoot.rootForm;
  }

  // Temporary fix for bug #1493131 - inspector has a different life cycle
  // than most other fronts because it is closely related to the toolbox.
  // TODO: remove once inspector is separated from the toolbox
  async getInspector(typeName) {
    // the front might have been destroyed and no longer have an actor ID
    if (this._inspector && this._inspector.actorID) {
      return this._inspector;
    }
    this._inspector = await getFront(this.client, "inspector", this.form);
    this.emit("inspector", this._inspector);
    return this._inspector;
  }

  // Run callback on every front of this type that currently exists, and on every
  // instantiation of front type in the future.
  onFront(typeName, callback) {
    const front = this.fronts.get(typeName);
    if (front) {
      return callback(front);
    }
    return this.on(typeName, callback);
  }

  // Get a Front for a target-scoped actor.
  // i.e. an actor served by RootActor.listTabs or RootActorActor.getTab requests
  async getFront(typeName) {
    let front = this.fronts.get(typeName);
    // the front might have been destroyed and no longer have an actor ID
    if (front && front.actorID || front && typeof front.then === "function") {
      return front;
    }
    front = getFront(this.client, typeName, this.form);
    this.fronts.set(typeName, front);
    // replace the placeholder with the instance of the front once it has loaded
    front = await front;
    this.emit(typeName, front);
    this.fronts.set(typeName, front);
    return front;
  }

  getCachedFront(typeName) {
    // do not wait for async fronts;
    const front = this.fronts.get(typeName);
    // ensure that the front is a front, and not async front
    if (front && front.actorID) {
      return front;
    }
    return null;
  }

  get client() {
    return this._client;
  }

  // Tells us if we are debugging content document
  // or if we are debugging chrome stuff.
  // Allows to controls which features are available against
  // a chrome or a content document.
  get chrome() {
    return this._chrome;
  }

  // Tells us if the related actor implements BrowsingContextTargetActor
  // interface and requires to call `attach` request before being used and
  // `detach` during cleanup.
  get isBrowsingContext() {
    return this._isBrowsingContext;
  }

  get name() {
    if (this.isAddon) {
      return this.form.name;
    }
    return this._title;
  }

  get url() {
    return this._url;
  }

  get isAddon() {
    return this.isLegacyAddon || this.isWebExtension;
  }

  get isWorkerTarget() {
    return this.activeTab && this.activeTab.typeName === "workerTarget";
  }

  get isLegacyAddon() {
    return !!(this.form && this.form.actor &&
      this.form.actor.match(/conn\d+\.addon(Target)?\d+/));
  }

  get isWebExtension() {
    return !!(this.form && this.form.actor && (
      this.form.actor.match(/conn\d+\.webExtension(Target)?\d+/) ||
      this.form.actor.match(/child\d+\/webExtension(Target)?\d+/)
    ));
  }

  get isContentProcess() {
    // browser content toolbox's form will be of the form:
    //   server0.conn0.content-process0/contentProcessTarget7
    // while xpcshell debugging will be:
    //   server1.conn0.contentProcessTarget7
    return !!(this.form && this.form.actor &&
      this.form.actor.match(/conn\d+\.(content-process\d+\/)?contentProcessTarget\d+/));
  }

  get isLocalTab() {
    return !!this._tab;
  }

  get isMultiProcess() {
    return !this.window;
  }

  get canRewind() {
    return this.activeTab && this.activeTab.traits.canRewind;
  }

  isReplayEnabled() {
    return Services.prefs.getBoolPref("devtools.recordreplay.mvp.enabled")
      && this.canRewind
      && this.isLocalTab;
  }

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
  }

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
  }

  /**
   * Attach the target and its console actor.
   *
   * This method will mainly call `attach` request on the target actor as well
   * as the console actor.
   * See DebuggerClient.attachTarget and DebuggerClient.attachConsole for more info.
   * It also starts listenings to events the target actor will start emitting
   * after being attached, like `tabDetached` and `frameUpdate`
   */
  attach() {
    if (this._attach) {
      return this._attach;
    }

    // Attach the target actor
    const attachBrowsingContextTarget = async () => {
      // Some BrowsingContextTargetFront are already instantiated and passed as
      // contructor's argument, like for ParentProcessTargetActor.
      // For them, we only need to attach them.
      // The call to attachTarget is to be removed once all Target are having a front
      // passed as contructor's argument.
      if (!this.activeTab) {
        const [, targetFront] = await this._client.attachTarget(this.form);
        this.activeTab = targetFront;
      } else {
        await this.activeTab.attach();
      }

      this.activeTab.on("tabNavigated", this._onTabNavigated);
      this._onFrameUpdate = packet => {
        this.emit("frame-update", packet);
      };
      this.activeTab.on("frameUpdate", this._onFrameUpdate);
    };

    // Attach the console actor
    const attachConsole = async () => {
      const [, consoleClient] = await this._client.attachConsole(
        this.form.consoleActor, []);
      this.activeConsole = consoleClient;

      this._onInspectObject = packet => this.emit("inspect-object", packet);
      this.activeConsole.on("inspectObject", this._onInspectObject);
    };

    this._attach = (async () => {
      // AddonTargetActor and ContentProcessTargetActor don't inherit from
      // BrowsingContextTargetActor (i.e. this.isBrowsingContext=false) and don't need
      // to be attached via DebuggerClient.attachTarget.
      if (this.isBrowsingContext) {
        await attachBrowsingContextTarget();

      // Addon Worker and Content process targets are the first targets to have their
      // front already instantiated. The plan is to have all targets to have their front
      // passed as constructor argument.
      } else if (this.isWorkerTarget || this.isLegacyAddon) {
        // Worker is the first front to be completely migrated to have only its attach
        // method being called from Target.attach. Other fronts should be refactored.
        await this.activeTab.attach();
      } else if (this.isContentProcess) {
        // ContentProcessTarget is the only one target without any attach request.
      } else {
        throw new Error(`Unsupported type of target. Expected target of one of the` +
          ` following types: BrowsingContext, ContentProcess, Worker or ` +
          `Addon (legacy).`);
      }

      // _setupRemoteListeners has to be called after the potential call to `attachTarget`
      // as it depends on `activeTab` which is set by this method.
      this._setupRemoteListeners();

      // But all target actor have a console actor to attach
      return attachConsole();
    })();

    return this._attach;
  }

  /**
   * Listen to the different events.
   */
  _setupListeners() {
    this.tab.addEventListener("TabClose", this);
    this.tab.ownerDocument.defaultView.addEventListener("unload", this);
    this.tab.addEventListener("TabRemotenessChange", this);
  }

  /**
   * Teardown event listeners.
   */
  _teardownListeners() {
    if (this._tab.ownerDocument.defaultView) {
      this._tab.ownerDocument.defaultView.removeEventListener("unload", this);
    }
    this._tab.removeEventListener("TabClose", this);
    this._tab.removeEventListener("TabRemotenessChange", this);
  }

  /**
   * Event listener for tabNavigated packet sent by activeTab's front.
   */
  _onTabNavigated(packet) {
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
      this.emit("will-navigate", event);
    } else {
      this.emit("navigate", event);
    }
  }

  /**
   * Setup listeners for remote debugging, updating existing ones as necessary.
   */
  _setupRemoteListeners() {
    this.client.addListener("closed", this.destroy);

    // For now, only browsing-context inherited actors are using a front,
    // for which events have to be listened on the front itself.
    // For other actors (ContentProcessTargetActor and AddonTargetActor), events should
    // still be listened directly on the client. This should be ultimately cleaned up to
    // only listen from a front by bug 1465635.
    if (this.activeTab) {
      this.activeTab.on("tabDetached", this.destroy);

      // These events should be ultimately listened from the thread client as
      // they are coming from it and no longer go through the Target Actor/Front.
      this._onSourceUpdated = packet => this.emit("source-updated", packet);
      this.activeTab.on("newSource", this._onSourceUpdated);
      this.activeTab.on("updatedSource", this._onSourceUpdated);
    } else {
      this._onTabDetached = (type, packet) => {
        // We have to filter message to ensure that this detach is for this tab
        if (packet.from == this.form.actor) {
          this.destroy();
        }
      };
      this.client.addListener("tabDetached", this._onTabDetached);

      this._onSourceUpdated = (type, packet) => this.emit("source-updated", packet);
      this.client.addListener("newSource", this._onSourceUpdated);
      this.client.addListener("updatedSource", this._onSourceUpdated);
    }
  }

  /**
   * Teardown listeners for remote debugging.
   */
  _teardownRemoteListeners() {
    // Remove listeners set in _setupRemoteListeners
    this.client.removeListener("closed", this.destroy);
    if (this.activeTab) {
      this.activeTab.off("tabDetached", this.destroy);
      this.activeTab.off("newSource", this._onSourceUpdated);
      this.activeTab.off("updatedSource", this._onSourceUpdated);
    } else {
      this.client.removeListener("tabDetached", this._onTabDetached);
      this.client.removeListener("newSource", this._onSourceUpdated);
      this.client.removeListener("updatedSource", this._onSourceUpdated);
    }

    // Remove listeners set in attachTarget
    if (this.activeTab) {
      this.activeTab.off("tabNavigated", this._onTabNavigated);
      this.activeTab.off("frameUpdate", this._onFrameUpdate);
    }

    // Remove listeners set in attachConsole
    if (this.activeConsole && this._onInspectObject) {
      this.activeConsole.off("inspectObject", this._onInspectObject);
    }
  }

  /**
   * Handle tabs events.
   */
  handleEvent(event) {
    switch (event.type) {
      case "TabClose":
      case "unload":
        this.destroy();
        break;
      case "TabRemotenessChange":
        this.onRemotenessChange();
        break;
    }
  }

  /**
   * Automatically respawn the toolbox when the tab changes between being
   * loaded within the parent process and loaded from a content process.
   * Process change can go in both ways.
   */
  onRemotenessChange() {
    // Responsive design do a crazy dance around tabs and triggers
    // remotenesschange events. But we should ignore them as at the end
    // the content doesn't change its remoteness.
    if (this._tab.isResponsiveDesignMode) {
      return;
    }

    // Save a reference to the tab as it will be nullified on destroy
    const tab = this._tab;
    const onToolboxDestroyed = async (target) => {
      if (target != this) {
        return;
      }
      gDevTools.off("toolbox-destroyed", target);

      // Recreate a fresh target instance as the current one is now destroyed
      const newTarget = await TargetFactory.forTab(tab);
      gDevTools.showToolbox(newTarget);
    };
    gDevTools.on("toolbox-destroyed", onToolboxDestroyed);
  }

  /**
   * Target is not alive anymore.
   */
  destroy() {
    // If several things call destroy then we give them all the same
    // destruction promise so we're sure to destroy only once
    if (this._destroyer) {
      return this._destroyer;
    }

    this._destroyer = (async () => {
      // Before taking any action, notify listeners that destruction is imminent.
      this.emit("close");

      for (let [, front] of this.fronts) {
        front = await front;
        await front.destroy();
      }

      if (this._tab) {
        this._teardownListeners();
      }

      this._teardownRemoteListeners();

      if (this.isLocalTab) {
        // We started with a local tab and created the client ourselves, so we
        // should close it.
        await this._client.close();
      } else if (this.activeTab) {
        // The client was handed to us, so we are not responsible for closing
        // it. We just need to detach from the tab, if already attached.
        // |detach| may fail if the connection is already dead, so proceed with
        // cleanup directly after this.
        try {
          await this.activeTab.detach();
        } catch (e) {
          console.warn(`Error while detaching target: ${e.message}`);
        }
      }

      this._cleanup();
    })();

    return this._destroyer;
  }

  /**
   * Clean up references to what this target points to.
   */
  _cleanup() {
    if (this._tab) {
      targets.delete(this._tab);
    } else {
      promiseTargets.delete(this.form);
    }

    this.activeTab = null;
    this.activeConsole = null;
    this._client = null;
    this._tab = null;
    this._attach = null;
    this._title = null;
    this._url = null;
  }

  toString() {
    const id = this._tab ? this._tab : (this.form && this.form.actor);
    return `Target:${id}`;
  }

  /**
   * Log an error of some kind to the tab's console.
   *
   * @param {String} text
   *                 The text to log.
   * @param {String} category
   *                 The category of the message.  @see nsIScriptError.
   * @returns {Promise}
   */
  logErrorInPage(text, category) {
    if (this.activeTab && this.activeTab.traits.logInPage) {
      const errorFlag = 0;
      return this.activeTab.logInPage({ text, category, flags: errorFlag });
    }
    return Promise.resolve();
  }

  /**
   * Log a warning of some kind to the tab's console.
   *
   * @param {String} text
   *                 The text to log.
   * @param {String} category
   *                 The category of the message.  @see nsIScriptError.
   * @returns {Promise}
   */
  logWarningInPage(text, category) {
    if (this.activeTab && this.activeTab.traits.logInPage) {
      const warningFlag = 1;
      return this.activeTab.logInPage({ text, category, flags: warningFlag });
    }
    return Promise.resolve();
  }
}
exports.Target = Target;
