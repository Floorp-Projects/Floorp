/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(this, "getFront", "devtools/shared/protocol", true);
loader.lazyRequireGetter(
  this,
  "getThreadOptions",
  "devtools/client/shared/thread-utils",
  true
);

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

      this.threadFront = null;

      // This flag will be set to true from:
      // - TabDescriptorFront getTarget(), for local tab targets
      // - descriptorFromURL(), for local targets (about:debugging)
      // - initToolbox(), for some test-only targets
      this.shouldCloseClient = false;

      this._client = client;

      // Cache of already created targed-scoped fronts
      // [typeName:string => Front instance]
      this.fronts = new Map();

      // `resource-available-form` events can be emitted by target actors before the
      // ResourceWatcher could add event listeners. The target front will cache those
      // events until the ResourceWatcher has added the listeners.
      this._resourceCache = [];
      this._onResourceAvailable = this._onResourceAvailable.bind(this);
      // In order to avoid destroying the `_resourceCache`, we need to call `super.on()`
      // instead of `this.on()`.
      super.on("resource-available-form", this._onResourceAvailable);

      this._addListeners();
    }

    on(eventName, listener) {
      if (eventName === "resource-available-form" && this._resourceCache) {
        this.off("resource-available-form", this._onResourceAvailable);
        for (const cache of this._resourceCache) {
          listener(cache);
        }
        this._resourceCache = null;
      }

      super.on(eventName, listener);
    }

    /**
     * Boolean flag to help distinguish Target Fronts from other Fronts.
     * As we are using a Mixin, we can't easily distinguish these fronts via instanceof().
     */
    get isTargetFront() {
      return true;
    }

    /**
     * Get the descriptor front for this target.
     *
     * TODO: Should be removed. This is misleading as only the top level target should have a descriptor.
     * This will return null for targets created by the Watcher actor and will still be defined
     * by targets created by RootActor methods (listSomething methods).
     */
    get descriptorFront() {
      if (this.isDestroyed()) {
        throw new Error("Descriptor already destroyed for target: " + this);
      }

      if (this.isWorkerTarget) {
        return this;
      }
      if (this.parentFront.typeName.endsWith("Descriptor")) {
        return this.parentFront;
      }
      throw new Error("Missing descriptor for target: " + this);
    }

    get targetType() {
      return this._targetType;
    }

    get isTopLevel() {
      return this._isTopLevel;
    }

    setTargetType(type) {
      this._targetType = type;
    }

    setIsTopLevel(isTopLevel) {
      this._isTopLevel = isTopLevel;
    }

    /**
     * Get the top level WatcherFront for this target.
     *
     * The targets should all ultimately be managed by a unique Watcher actor,
     * created from the unique Descriptor actor which is passed to the Toolbox.
     * For now, the top level target is still created by the top level Descriptor,
     * but it is also meant to be created by the Watcher.
     *
     * @return {TargetMixin} the parent target.
     */
    getWatcherFront() {
      // All additional frame targets are spawn by the WatcherActor and are managed by it.
      if (this.parentFront.typeName == "watcher") {
        return this.parentFront;
      }

      // Otherwise, for top level targets, the parent front is a Descriptor, from which we can retrieve the Watcher.
      // TODO: top level target should also be exposed by the Watcher actor, like any target.
      if (
        this.parentFront.typeName.endsWith("Descriptor") &&
        this.parentFront.traits &&
        this.parentFront.traits.watcher
      ) {
        return this.parentFront.getWatcher();
      }

      // For WebExtension, the descriptor doesn't expose a watcher yet (See Bug 1675456).
      return null;
    }

    /**
     * Get the immediate parent target for this target.
     *
     * @return {TargetMixin} the parent target.
     */
    async getParentTarget() {
      // We now support frames watching via watchTargets for Tab and Process descriptors.
      const watcherFront = await this.getWatcherFront();
      if (watcherFront) {
        // Safety check, in theory all watcher should support frames. We should be able
        // to remove this as part of Bug 1680280.
        if (watcherFront.traits.frame) {
          // Retrieve the Watcher, which manage all the targets and should already have a reference to
          // to the parent target.
          return watcherFront.getParentBrowsingContextTarget(
            this.browsingContextID
          );
        }
        return null;
      }

      if (this.parentFront.getParentTarget) {
        return this.parentFront.getParentTarget();
      }

      // Other targets, like WebExtensions, don't have a Watcher yet, nor do expose `getParentTarget`.
      // We can't fetch parent target yet for these targets.
      return null;
    }

    /**
     * Get the target for the given Browsing Context ID.
     *
     * @return {TargetMixin} the requested target.
     */
    async getBrowsingContextTarget(browsingContextID) {
      // Tab and Process Descriptors expose a Watcher, which is creating the
      // targets and should be used to fetch any.
      const watcherFront = await this.getWatcherFront();
      if (watcherFront) {
        // Safety check, in theory all watcher should support frames.
        if (watcherFront.traits.frame) {
          return watcherFront.getBrowsingContextTarget(browsingContextID);
        }
        return null;
      }

      // For descriptors which don't expose a watcher (e.g. WebExtension)
      // we used to call RootActor::getBrowsingContextDescriptor, but it was
      // removed in FF77.
      // Support for watcher in WebExtension descriptors is Bug 1644341.
      throw new Error(
        `Unable to call getBrowsingContextTarget for ${this.actorID}`
      );
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

    get isLocalTab() {
      // Worker Target is also the Descriptor,
      // so avoid infinite loop.
      if (this.isWorkerTarget) {
        return false;
      }
      return !!this.descriptorFront?.isLocalTab;
    }

    get localTab() {
      // Worker Target is also the Descriptor,
      // so avoid infinite loop.
      if (this.isWorkerTarget) {
        return null;
      }
      return this.descriptorFront?.localTab || null;
    }

    // Get a promise of the RootActor's form
    get root() {
      return this.client.mainRoot.rootForm;
    }

    // Get a Front for a target-scoped actor.
    // i.e. an actor served by RootActor.listTabs or RootActorActor.getTab requests
    async getFront(typeName) {
      let front = this.fronts.get(typeName);
      if (front) {
        // XXX: This is typically the kind of spot where switching to
        // `isDestroyed()` is complicated, because `front` is not necessarily a
        // Front...
        const isFrontInitializing = typeof front.then === "function";
        const isFrontAlive = !isFrontInitializing && !front.isDestroyed();
        if (isFrontInitializing || isFrontAlive) {
          return front;
        }
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
      // XXX Remove the check on `workerDescriptor` as part of Bug 1667404.
      return (
        this.typeName === "workerTarget" || this.typeName === "workerDescriptor"
      );
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

      if (this.isDestroyedOrBeingDestroyed()) {
        return;
      }

      // Calling startListeners will populate the traits as it's the first request we
      // make to the front.
      await consoleFront.startListeners([]);

      this._onInspectObject = packet => this.emit("inspect-object", packet);
      this.removeOnInspectObjectListener = consoleFront.on(
        "inspectObject",
        this._onInspectObject
      );
    }

    /**
     * This method attaches the target and then attaches its related thread, sending it
     * the options it needs (e.g. breakpoints, pause on exception setting, …).
     * This function can be called multiple times, it will only perform the actual
     * initialization process once; on subsequent call the original promise (_onThreadInitialized)
     * will be returned.
     *
     * @param {TargetList} targetList
     * @returns {Promise} A promise that resolves once the thread is attached and resumed.
     */
    attachAndInitThread(targetList) {
      if (this._onThreadInitialized) {
        return this._onThreadInitialized;
      }

      this._onThreadInitialized = this._attachAndInitThread(targetList);
      return this._onThreadInitialized;
    }

    /**
     * This method attach the target and then attach its related thread, sending it the
     * options it needs (e.g. breakpoints, pause on exception setting, …)
     *
     * @private
     * @param {TargetList} targetList
     * @returns {Promise} A promise that resolves once the thread is attached and resumed.
     */
    async _attachAndInitThread(targetList) {
      // If the target is destroyed or soon will be, don't go further
      if (this.isDestroyedOrBeingDestroyed()) {
        return;
      }

      // WorkerTargetFront don't have an attach function as the related console and thread
      // actors are created right away (from devtools/server/startup/worker.js)
      if (this.attach) {
        await this.attach();
      }

      const isBrowserToolbox = targetList.targetFront.isParentProcess;
      const isNonTopLevelFrameTarget =
        !this.isTopLevel && this.targetType === targetList.TYPES.FRAME;

      if (isBrowserToolbox && isNonTopLevelFrameTarget) {
        // In the BrowserToolbox, non-top-level frame targets are already
        // debugged via content-process targets.
        // Do not attach the thread here, as it was already done by the
        // corresponding content-process target.
        return;
      }

      // Avoid attaching the thread actor in the browser console
      // in order to avoid trigerring any type of breakpoint.
      if (targetList.targetFront.isBrowserConsoleTarget) {
        return;
      }

      const options = await getThreadOptions();
      // If the target is destroyed or soon will be, don't go further
      if (this.isDestroyedOrBeingDestroyed()) {
        return;
      }
      const threadFront = await this.attachThread(options);

      // @backward-compat { version 86 } ThreadActor.attach no longer pause the thread,
      //                                 so that we no longer have to resume.
      // Once 86 is in release, we can remove the rest of this method.
      if (this.getTrait("noPauseOnThreadActorAttach")) {
        return;
      }
      try {
        if (this.isDestroyedOrBeingDestroyed() || threadFront.isDestroyed()) {
          return;
        }
        await threadFront.resume();
      } catch (ex) {
        if (ex.error === "wrongOrder") {
          targetList.emit("target-thread-wrong-order-on-resume");
        } else {
          throw ex;
        }
      }
    }

    async attachThread(options = {}) {
      if (!this.targetForm || !this.targetForm.threadActor) {
        throw new Error(
          "TargetMixin sub class should set targetForm.threadActor before calling " +
            "attachThread"
        );
      }
      this.threadFront = await this.getFront("thread");

      // Avoid attaching if the thread actor was already attached on target creation from the server side.
      // This doesn't include:
      // * targets that aren't yet supported by the Watcher (like web extensions),
      // * workers, which still use a unique codepath for thread actor attach
      // * all targets when connecting to an older server
      // @backward-compat { version 87 } If all targets are supported by watcher actor, and workers no longer use
      //                                 its unique attach sequence, we can assume the thread front is always attached.
      const isAttached =
        this.getTrait("supportsThreadActorIsAttached") &&
        (await this.threadFront.isAttached());
      if (isAttached) {
        // If the Thread actor has already been attached from the server side
        // by the Watcher Actor, we still have to pass options that aren't yet managed via
        // the Watcher actor's addWatcherDataEntry codepath (bug 1687261).
        await this.threadFront.reconfigure(options);
        return this.threadFront;
      }
      if (
        this.isDestroyedOrBeingDestroyed() ||
        this.threadFront.isDestroyed()
      ) {
        return this.threadFront;
      }

      await this.threadFront.attach(options);

      return this.threadFront;
    }

    /**
     * Setup listeners.
     */
    _addListeners() {
      this.client.on("closed", this.destroy);

      // `tabDetached` is sent by all target targets types: frame, process and workers.
      // This is sent when the target is destroyed:
      // * the target context destroys itself (the tab closes for ex, or the worker shuts down)
      //   in this case, it may be the connector that send this event in the name of the target actor
      // * the target actor is destroyed, but the target context stays up and running (for ex, when we call Watcher.unwatchTargets)
      // * the DevToolsServerConnection closes (client closes the connection)
      this.on("tabDetached", this.destroy);
    }

    /**
     * Teardown listeners.
     */
    _removeListeners() {
      // Remove listeners set in _addListeners
      if (this.client) {
        this.client.off("closed", this.destroy);
      }
      this.off("tabDetached", this.destroy);

      // Remove listeners set in attachConsole
      if (this.removeOnInspectObjectListener) {
        this.removeOnInspectObjectListener();
        this.removeOnInspectObjectListener = null;
      }
    }

    isDestroyedOrBeingDestroyed() {
      return this.isDestroyed() || this._destroyer;
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
      // If the target is being attached, try to wait until it's done, to prevent having
      // pending connection to the server when the toolbox is destroyed.
      if (this._onThreadInitialized) {
        try {
          await this._onThreadInitialized;
        } catch (e) {
          // We might still get into cases where attaching fails (e.g. the worker we're
          // trying to attach to is already closed). Since the target is being destroyed,
          // we don't need to do anything special here.
        }
      }

      for (let [name, front] of this.fronts) {
        try {
          // If a Front with an async initialize method is still being instantiated,
          // we should wait for completion before trying to destroy it.
          if (front instanceof Promise) {
            front = await front;
          }
          front.destroy();
        } catch (e) {
          console.warn("Error while destroying front:", name, e);
        }
      }

      this._removeListeners();

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
      } else if (this.detach && !this.isDestroyed()) {
        // The client was handed to us, so we are not responsible for closing
        // it. We just need to detach from the tab, if already attached.
        // |detach| may fail if the connection is already dead, so proceed with
        // cleanup directly after this.
        try {
          await this.detach();
        } catch (e) {
          this.logDetachError(e);
        }
      }

      // This event should be emitted before calling super.destroy(), because
      // super.destroy() will remove all event listeners attached to this front.
      this.emit("target-destroyed");

      // Do that very last in order to let a chance to dispatch `detach` requests.
      super.destroy();

      this._cleanup();
    }

    /**
     * Detach can fail under regular circumstances, if the target was already
     * destroyed on the server side. All target fronts should handle detach
     * error logging in similar ways so this might be used by subclasses
     * with custom detach() implementations.
     *
     * @param {Error} e
     *        The real error object.
     * @param {String} targetType
     *        The type of the target front ("worker", "browsing-context", ...)
     */
    logDetachError(e, targetType) {
      const ignoredError =
        e?.message.includes("noSuchActor") ||
        e?.message.includes("Connection closed");

      // Silence exceptions for already destroyed actors and fronts:
      // - "noSuchActor" errors from the server
      // - "Connection closed" errors from the client, when purging requests
      if (ignoredError) {
        return;
      }

      // Properly log any other error.
      const message = targetType
        ? `Error while detaching the ${targetType} target:`
        : "Error while detaching target:";
      console.warn(message, e);
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

    _onResourceAvailable(resources) {
      if (this._resourceCache) {
        this._resourceCache.push(resources);
      }
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
