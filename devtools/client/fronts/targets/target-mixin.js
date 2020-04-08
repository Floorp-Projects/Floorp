/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(this, "getFront", "devtools/shared/protocol", true);

/**
 * A Target represents a debuggable context. It can be a browser tab, a tab on
 * a remote device, like a tab on Firefox for Android. But it can also be an add-on,
 * as well as firefox parent process, or just one of its content process.
 * A Target is related to a given TargetActor, for which we derive this class.
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
 *
 * Optional events only dispatched by BrowsingContextTarget:
 * - will-navigate: The target window will navigate to a different URL
 * - navigate: The target window has navigated to a different URL
 */
function TargetMixin(parentClass) {
  class Target extends parentClass {
    constructor(client, targetFront, parentFront) {
      super(client, targetFront, parentFront);

      this._forceChrome = false;

      this.destroy = this.destroy.bind(this);
      this._onNewSource = this._onNewSource.bind(this);

      this.threadFront = null;

      // This promise is exposed to consumers that want to wait until the thread
      // front is available and attached.
      this.onThreadAttached = new Promise(
        r => (this._resolveOnThreadAttached = r)
      );

      // By default, we close the DevToolsClient of local tabs which
      // are instanciated from TargetFactory module.
      // This flag will also be set on local targets opened from about:debugging,
      // for which a dedicated DevToolsClient is also created.
      this.shouldCloseClient = this.isLocalTab;

      this._client = client;

      // Cache of already created targed-scoped fronts
      // [typeName:string => Front instance]
      this.fronts = new Map();

      this._setupRemoteListeners();
    }

    get descriptorFront() {
      return this.parentFront;
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
      if (
        this._protocolDescription &&
        this._protocolDescription.types[actorName]
      ) {
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
      if (this.targetForm) {
        return !!this.targetForm[actorName + "Actor"];
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
        if (desc?.methods) {
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
      if (this.targetForm.traits && traitName in this.targetForm.traits) {
        return this.targetForm.traits[traitName];
      }

      return this.client.traits[traitName];
    }

    /**
     * The following getters: isLocalTab, localTab, ... will be overriden for
     * local tabs by some code in devtools/client/fronts/targets/local-tab.js.
     * They are all specific to local tabs, i.e. when you are debugging a tab of
     * the current Firefox instance.
     */
    get isLocalTab() {
      return false;
    }

    get localTab() {
      return null;
    }

    // Get a promise of the RootActor's form
    get root() {
      return this.client.mainRoot.rootForm;
    }

    // Get a Front for a target-scoped actor.
    // i.e. an actor served by RootActor.listTabs or RootActorActor.getTab requests
    async getFront(typeName) {
      let front = this.fronts.get(typeName);
      // the front might have been destroyed and no longer have an actor ID
      if (front?.actorID || (front && typeof front.then === "function")) {
        return front;
      }
      front = getFront(this.client, typeName, this.targetForm, this);
      this.fronts.set(typeName, front);
      // replace the placeholder with the instance of the front once it has loaded
      front = await front;
      this.fronts.set(typeName, front);
      return front;
    }

    getCachedFront(typeName) {
      // do not wait for async fronts;
      const front = this.fronts.get(typeName);
      // ensure that the front is a front, and not async front
      if (front?.actorID) {
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
      return (
        this.isAddon ||
        this.isContentProcess ||
        this.isParentProcess ||
        this.isWindowTarget ||
        this._forceChrome
      );
    }

    forceChrome() {
      this._forceChrome = true;
    }

    // Tells us if the related actor implements BrowsingContextTargetActor
    // interface and requires to call `attach` request before being used and
    // `detach` during cleanup.
    get isBrowsingContext() {
      return this.typeName === "browsingContextTarget";
    }

    get name() {
      if (this.isAddon || this.isContentProcess) {
        return this.targetForm.name;
      }
      return this.title;
    }

    get title() {
      return this._title || this.url;
    }

    get url() {
      return this._url;
    }

    get isAddon() {
      return this.isLegacyAddon || this.isWebExtension;
    }

    get isWorkerTarget() {
      return this.typeName === "workerTarget";
    }

    get isLegacyAddon() {
      return !!(
        this.targetForm &&
        this.targetForm.actor &&
        this.targetForm.actor.match(/conn\d+\.addon(Target)?\d+/)
      );
    }

    get isWebExtension() {
      return !!(
        this.targetForm &&
        this.targetForm.actor &&
        (this.targetForm.actor.match(/conn\d+\.webExtension(Target)?\d+/) ||
          this.targetForm.actor.match(/child\d+\/webExtension(Target)?\d+/))
      );
    }

    get isContentProcess() {
      // browser content toolbox's form will be of the form:
      //   server0.conn0.content-process0/contentProcessTarget7
      // while xpcshell debugging will be:
      //   server1.conn0.contentProcessTarget7
      return !!(
        this.targetForm &&
        this.targetForm.actor &&
        this.targetForm.actor.match(
          /conn\d+\.(content-process\d+\/)?contentProcessTarget\d+/
        )
      );
    }

    get isParentProcess() {
      return !!(
        this.targetForm &&
        this.targetForm.actor &&
        this.targetForm.actor.match(/conn\d+\.parentProcessTarget\d+/)
      );
    }

    get isWindowTarget() {
      return !!(
        this.targetForm &&
        this.targetForm.actor &&
        this.targetForm.actor.match(/conn\d+\.chromeWindowTarget\d+/)
      );
    }

    get isMultiProcess() {
      return !this.window;
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

    // Attach the console actor
    async attachConsole() {
      const consoleFront = await this.getFront("console");
      await consoleFront.startListeners([]);

      this._onInspectObject = packet => this.emit("inspect-object", packet);
      this.removeOnInspectObjectListener = consoleFront.on(
        "inspectObject",
        this._onInspectObject
      );
    }

    /**
     * Attach to thread actor.
     *
     * This depends on having the sub-class to set the thread actor ID in `targetForm`.
     *
     * @param object options
     *        Configuration options.
     */
    async attachThread(options = {}) {
      if (!this.targetForm || !this.targetForm.threadActor) {
        throw new Error(
          "TargetMixin sub class should set targetForm.threadActor before calling " +
            "attachThread"
        );
      }
      this.threadFront = await this.getFront("thread");
      await this.threadFront.attach(options);

      this.threadFront.on("newSource", this._onNewSource);

      // Resolve the onThreadAttached promise so that consumers that need to
      // wait for the thread to be attached can resume.
      this._resolveOnThreadAttached();

      return this.threadFront;
    }

    // Listener for "newSource" event fired by the thread actor
    _onNewSource(packet) {
      this.emit("source-updated", packet);
    }

    /**
     * Setup listeners for remote debugging, updating existing ones as necessary.
     */
    _setupRemoteListeners() {
      this.client.on("closed", this.destroy);

      this.on("tabDetached", this.destroy);
    }

    /**
     * Teardown listeners for remote debugging.
     */
    _teardownRemoteListeners() {
      // Remove listeners set in _setupRemoteListeners
      if (this.client) {
        this.client.off("closed", this.destroy);
      }
      this.off("tabDetached", this.destroy);

      // Remove listeners set in attachThread
      if (this.threadFront) {
        this.threadFront.off("newSource", this._onNewSource);
      }

      // Remove listeners set in attachConsole
      if (this.removeOnInspectObjectListener) {
        this.removeOnInspectObjectListener();
        this.removeOnInspectObjectListener = null;
      }
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

      // This pattern allows to immediately return the destroyer promise.
      // See Bug 1602727 for more details.
      let destroyerResolve;
      this._destroyer = new Promise(r => (destroyerResolve = r));
      this._destroyTarget().then(destroyerResolve);

      return this._destroyer;
    }

    async _destroyTarget() {
      // Before taking any action, notify listeners that destruction is imminent.
      this.emit("close");

      for (let [, front] of this.fronts) {
        // If a Front with an async initialize method is still being instantiated,
        // we should wait for completion before trying to destroy it.
        if (front instanceof Promise) {
          front = await front;
        }
        front.destroy();
      }

      this._teardownRemoteListeners();

      this.threadFront = null;

      if (this.shouldCloseClient) {
        try {
          await this._client.close();
        } catch (e) {
          // Ignore any errors while closing, since there is not much that can be done
          // at this point.
          console.warn("Error while closing client:", e);
        }

        // Not all targets supports attach/detach. For example content process doesn't.
        // Also ensure that the front is still active before trying to do the request.
      } else if (this.detach && this.actorID) {
        // The client was handed to us, so we are not responsible for closing
        // it. We just need to detach from the tab, if already attached.
        // |detach| may fail if the connection is already dead, so proceed with
        // cleanup directly after this.
        try {
          await this.detach();
        } catch (e) {
          console.warn("Error while detaching target:", e);
        }
      }

      // Do that very last in order to let a chance to dispatch `detach` requests.
      super.destroy();

      this._cleanup();
    }

    /**
     * Clean up references to what this target points to.
     */
    _cleanup() {
      this.threadFront = null;
      this._client = null;

      // All target front subclasses set this variable in their `attach` method.
      // None of them overload destroy, so clean this up from here.
      this._attach = null;

      this._title = null;
      this._url = null;
    }

    toString() {
      const id = this.targetForm ? this.targetForm.actor : null;
      return `Target:${id}`;
    }

    dumpPools() {
      // NOTE: dumpPools is defined in the Thread actor to avoid
      // adding it to multiple target specs and actors.
      return this.threadFront.dumpPools();
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
      if (this.traits.logInPage) {
        const errorFlag = 0;
        return this.logInPage({ text, category, flags: errorFlag });
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
      if (this.traits.logInPage) {
        const warningFlag = 1;
        return this.logInPage({ text, category, flags: warningFlag });
      }
      return Promise.resolve();
    }
  }
  return Target;
}
exports.TargetMixin = TargetMixin;
