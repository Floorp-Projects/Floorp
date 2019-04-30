/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// We are requiring a module from client whereas this module is from shared.
// This shouldn't happen, but Fronts should rather be part of client anyway.
// Otherwise gDevTools is only used for local tabs and should propably only
// used by a subclass, specific to local tabs.
loader.lazyRequireGetter(this, "gDevTools", "devtools/client/framework/devtools", true);
loader.lazyRequireGetter(this, "TargetFactory", "devtools/client/framework/target", true);
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
    constructor(client, form) {
      super(client, form);

      this._forceChrome = false;

      this.destroy = this.destroy.bind(this);
      this._onNewSource = this._onNewSource.bind(this);

      this.activeConsole = null;
      this.threadClient = null;

      this._client = client;

      // Cache of already created targed-scoped fronts
      // [typeName:string => Front instance]
      this.fronts = new Map();
      // Temporary fix for bug #1493131 - inspector has a different life cycle
      // than most other fronts because it is closely related to the toolbox.
      // TODO: remove once inspector is separated from the toolbox
      this._inspector = null;

      this._setupRemoteListeners();
    }

    attachTab(tab) {
      // When debugging local tabs, we also have a reference to the Firefox tab
      // This is used to:
      // * distinguish local tabs from remote (see target.isLocalTab)
      // * being able to hookup into Firefox UI (see Hosts)
      this._tab = tab;
      this._setupListeners();
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
      if (this._protocolDescription && this._protocolDescription.types[actorName]) {
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
      if (this.targetForm.traits && traitName in this.targetForm.traits) {
        return this.targetForm.traits[traitName];
      }

      return this.client.traits[traitName];
    }

    get tab() {
      return this._tab;
    }

    // Get a promise of the RootActor's form
    get root() {
      return this.client.mainRoot.rootForm;
    }

    // Temporary fix for bug #1493131 - inspector has a different life cycle
    // than most other fronts because it is closely related to the toolbox.
    // TODO: remove once inspector is separated from the toolbox
    async getInspector() {
      // the front might have been destroyed and no longer have an actor ID
      if (this._inspector && this._inspector.actorID) {
        return this._inspector;
      }
      this._inspector = await getFront(this.client, "inspector", this.targetForm);
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
      if ((front && front.actorID) || (front && typeof front.then === "function")) {
        return front;
      }
      front = getFront(this.client, typeName, this.targetForm);
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
      if (this.isAddon) {
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

    get isLocalTab() {
      return !!this._tab;
    }

    get isMultiProcess() {
      return !this.window;
    }

    get canRewind() {
      return this.traits.canRewind;
    }

    isReplayEnabled() {
      return this.canRewind && this.isLocalTab;
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
     * Similar to the above get contentPrincipal(), the get csp()
     * returns the CSP which should be used for opening links.
     */
    get csp() {
      if (!this.isLocalTab) {
        return null;
      }
      return this.tab.linkedBrowser.csp;
    }

    // Attach the console actor
    async attachConsole() {
      this.activeConsole = await this.getFront("console");
      await this.activeConsole.startListeners([]);

      this._onInspectObject = packet => this.emit("inspect-object", packet);
      this.activeConsole.on("inspectObject", this._onInspectObject);
    }

    /**
     * Attach to thread actor.
     *
     * This depends on having the sub-class to set the thread actor ID in `_threadActor`.
     *
     * @param object options
     *        Configuration options.
     */
    async attachThread(options = {}) {
      if (!this._threadActor) {
        throw new Error(
          "TargetMixin sub class should set _threadActor before calling " + "attachThread"
        );
      }
      const [response, threadClient] = await this._client.attachThread(
        this._threadActor,
        options
      );
      this.threadClient = threadClient;

      this.threadClient.addListener("newSource", this._onNewSource);

      return [response, threadClient];
    }

    // Listener for "newSource" event fired by the thread actor
    _onNewSource(type, packet) {
      this.emit("source-updated", packet);
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
     * Setup listeners for remote debugging, updating existing ones as necessary.
     */
    _setupRemoteListeners() {
      this.client.addListener("closed", this.destroy);

      this.on("tabDetached", this.destroy);
    }

    /**
     * Teardown listeners for remote debugging.
     */
    _teardownRemoteListeners() {
      // Remove listeners set in _setupRemoteListeners
      this.client.removeListener("closed", this.destroy);
      this.off("tabDetached", this.destroy);

      // Remove listeners set in attachThread
      if (this.threadClient) {
        this.threadClient.removeListener("newSource", this._onNewSource);
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
      const onToolboxDestroyed = async target => {
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
            console.warn(`Error while detaching target: ${e.message}`);
          }
        }

        if (this.threadClient) {
          try {
            await this.threadClient.detach();
          } catch (e) {
            console.warn(`Error while detaching the thread front: ${e.message}`);
          }
        }

        // Do that very last in order to let a chance to dispatch `detach` requests.
        super.destroy();

        this._cleanup();
      })();

      return this._destroyer;
    }

    /**
     * Clean up references to what this target points to.
     */
    _cleanup() {
      this.activeConsole = null;
      this.threadClient = null;
      this._client = null;
      this._tab = null;

      // All target front subclasses set this variable in their `attach` method.
      // None of them overload destroy, so clean this up from here.
      this._attach = null;

      this._title = null;
      this._url = null;
    }

    toString() {
      const id = this._tab ? this._tab : this.targetForm && this.targetForm.actor;
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
