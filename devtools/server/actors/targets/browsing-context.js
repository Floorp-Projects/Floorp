/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global XPCNativeWrapper */

// protocol.js uses objects as exceptions in order to define
// error packets.
/* eslint-disable no-throw-literal */

/*
 * BrowsingContextTargetActor is an abstract class used by target actors that hold
 * documents, such as frames, chrome windows, etc.
 *
 * This class is extended by FrameTargetActor, ParentProcessTargetActor.
 *
 * See devtools/docs/backend/actor-hierarchy.md for more details.
 *
 * For performance matters, this file should only be loaded in the targeted context's
 * process. For example, it shouldn't be evaluated in the parent process until we try to
 * debug a document living in the parent process.
 */

var { Ci, Cu, Cr, Cc } = require("chrome");
var Services = require("Services");
const ChromeUtils = require("ChromeUtils");
var { ActorRegistry } = require("devtools/server/actors/utils/actor-registry");
var DevToolsUtils = require("devtools/shared/DevToolsUtils");
var { assert } = DevToolsUtils;
var {
  SourcesManager,
} = require("devtools/server/actors/utils/sources-manager");
var makeDebugger = require("devtools/server/actors/utils/make-debugger");
const InspectorUtils = require("InspectorUtils");
const Targets = require("devtools/server/actors/targets/index");
const { TargetActorRegistry } = ChromeUtils.import(
  "resource://devtools/server/actors/targets/target-actor-registry.jsm"
);

const EXTENSION_CONTENT_JSM = "resource://gre/modules/ExtensionContent.jsm";

const { Actor, Pool } = require("devtools/shared/protocol");
const {
  LazyPool,
  createExtraActors,
} = require("devtools/shared/protocol/lazy-pool");
const {
  browsingContextTargetSpec,
} = require("devtools/shared/specs/targets/browsing-context");
const Resources = require("devtools/server/actors/resources/index");
const TargetActorMixin = require("devtools/server/actors/targets/target-actor-mixin");

loader.lazyRequireGetter(
  this,
  ["ThreadActor", "unwrapDebuggerObjectGlobal"],
  "devtools/server/actors/thread",
  true
);
loader.lazyRequireGetter(
  this,
  "WorkerDescriptorActorList",
  "devtools/server/actors/worker/worker-descriptor-actor-list",
  true
);
loader.lazyImporter(this, "ExtensionContent", EXTENSION_CONTENT_JSM);

loader.lazyRequireGetter(
  this,
  ["StyleSheetActor", "getSheetText"],
  "devtools/server/actors/style-sheet",
  true
);

function getWindowID(window) {
  return window.windowGlobalChild.innerWindowId;
}

function getDocShellChromeEventHandler(docShell) {
  let handler = docShell.chromeEventHandler;
  if (!handler) {
    try {
      // Toplevel xul window's docshell doesn't have chromeEventHandler
      // attribute. The chrome event handler is just the global window object.
      handler = docShell.domWindow;
    } catch (e) {
      // ignore
    }
  }
  return handler;
}

/**
 * Helper to retrieve all children docshells of a given docshell.
 *
 * Given that docshell interfaces can only be used within the same process,
 * this only returns docshells for children documents that runs in the same process
 * as the given docshell.
 */
function getChildDocShells(parentDocShell) {
  return parentDocShell.browsingContext
    .getAllBrowsingContextsInSubtree()
    .filter(browsingContext => {
      // Filter out browsingContext which don't expose any docshell (e.g. remote frame)
      return browsingContext.docShell;
    })
    .map(browsingContext => {
      // Map BrowsingContext to DocShell
      return browsingContext.docShell;
    });
}

exports.getChildDocShells = getChildDocShells;

/**
 * Browser-specific actors.
 */

function getInnerId(window) {
  return window.windowGlobalChild.innerWindowId;
}

const browsingContextTargetPrototype = {
  /**
   * BrowsingContextTargetActor is an abstract class used by target actors that
   * hold documents, such as frames, chrome windows, etc.  The term "browsing
   * context" is defined in the HTML spec as "an environment in which `Document`
   * objects are presented to the user".  In Gecko, this means a browsing context
   * is a `docShell`.
   *
   * The main goal of this class is to expose the target-scoped actors being registered
   * via `ActorRegistry.registerModule` and manage their lifetimes. In addition, this
   * class also tracks the lifetime of the targeted browsing context.
   *
   * ### Main requests:
   *
   * `attach`/`detach` requests:
   *  - start/stop document watching:
   *    Starts watching for new documents and emits `tabNavigated` and
   *    `frameUpdate` over RDP.
   *  - retrieve the thread actor:
   *    Instantiates a ThreadActor that can be later attached to in order to
   *    debug JS sources in the document.
   * `switchToFrame`:
   *  Change the targeted document of the whole actor, and its child target-scoped actors
   *  to an iframe or back to its original document.
   *
   * Most properties (like `chromeEventHandler` or `docShells`) are meant to be
   * used by the various child target actors.
   *
   * ### RDP events:
   *
   *  - `tabNavigated`:
   *    Sent when the browsing context is about to navigate or has just navigated
   *    to a different document.
   *    This event contains the following attributes:
   *     * url (string)
   *       The new URI being loaded.
   *     * nativeConsoleAPI (boolean)
   *       `false` if the console API of the page has been overridden (e.g. by Firebug)
   *       `true`  if the Gecko implementation is used
   *     * state (string)
   *       `start` if we just start requesting the new URL
   *       `stop`  if the new URL is done loading
   *     * isFrameSwitching (boolean)
   *       Indicates the event is dispatched when switching the actor context to a
   *       different frame. When we switch to an iframe, there is no document
   *       load. The targeted document is most likely going to be already done
   *       loading.
   *     * title (string)
   *       The document title being loaded. (sent only on state=stop)
   *
   *  - `frameUpdate`:
   *    Sent when there was a change in the child frames contained in the document
   *    or when the actor's context was switched to another frame.
   *    This event can have four different forms depending on the type of change:
   *    * One or many frames are updated:
   *      { frames: [{ id, url, title, parentID }, ...] }
   *    * One frame got destroyed:
   *      { frames: [{ id, destroy: true }]}
   *    * All frames got destroyed:
   *      { destroyAll: true }
   *    * We switched the context of the actor to a specific frame:
   *      { selected: #id }
   *
   * ### Internal, non-rdp events:
   *
   * Various events are also dispatched on the actor itself without being sent to
   * the client. They all relate to the documents tracked by this target actor
   * (its main targeted document, but also any of its iframes):
   *  - will-navigate
   *    This event fires once navigation starts. All pending user prompts are
   *    dealt with, but it is fired before the first request starts.
   *  - navigate
   *    This event is fired once the document's readyState is "complete".
   *  - window-ready
   *    This event is fired in various distinct scenarios:
   *     * When a new Window object is crafted, equivalent of `DOMWindowCreated`.
   *       It is dispatched before any page script is executed.
   *     * We will have already received a window-ready event for this window
   *       when it was created, but we received a window-destroyed event when
   *       it was frozen into the bfcache, and now the user navigated back to
   *       this page, so it's now live again and we should resume handling it.
   *     * For each existing document, when an `attach` request is received.
   *       At this point scripts in the page will be already loaded.
   *     * When `swapFrameLoaders` is used, such as with moving browsing contexts
   *       between windows or toggling Responsive Design Mode.
   *  - window-destroyed
   *    This event is fired in two cases:
   *     * When the window object is destroyed, i.e. when the related document
   *       is garbage collected. This can happen when the browsing context is
   *       closed or the iframe is removed from the DOM.
   *       It is equivalent of `inner-window-destroyed` event.
   *     * When the page goes into the bfcache and gets frozen.
   *       The equivalent of `pagehide`.
   *  - changed-toplevel-document
   *    This event fires when we switch the actor's targeted document
   *    to one of its iframes, or back to its original top document.
   *    It is dispatched between window-destroyed and window-ready.
   *  - stylesheet-added
   *    This event is fired when a StyleSheetActor is created.
   *    It contains the following attribute:
   *     * actor (StyleSheetActor) The created actor.
   *
   * Note that *all* these events are dispatched in the following order
   * when we switch the context of the actor to a given iframe:
   *  - will-navigate
   *  - window-destroyed
   *  - changed-toplevel-document
   *  - window-ready
   *  - navigate
   *
   * This class is subclassed by FrameTargetActor and others.
   * Subclasses are expected to implement a getter for the docShell property.
   *
   * @param connection DevToolsServerConnection
   *        The conection to the client.
   * @param options Object
   *        Object with following attributes:
   *        - docShell nsIDocShell
   *          The |docShell| for the debugged frame.
   *        - doNotFireFrameUpdates Boolean
   *          If true, omit emitting `frameUpdate` events. This is only useful
   *          for the top level target, in order to populate the toolbox iframe selector dropdown.
   *          But we can avoid sending these RDP messages for any additional remote target.
   *        - followWindowGlobalLifeCycle Boolean
   *          If true, the target actor will only inspect the current WindowGlobal (and its children windows).
   *          But won't inspect next document loaded in the same BrowsingContext.
   *          The actor will behave more like a WindowGlobalTarget rather than a BrowsingContextTarget.
   *          We may eventually switch everything to this, i.e. uses only WindowGlobalTarget.
   *          But for now, we restrict this behavior to remoted iframes.
   *        - isTopLevelTarget Boolean
   *          Should be set to true for all top-level targets. A top level target
   *          is the topmost target of a DevTools "session". For instance for a local
   *          tab toolbox, the FrameTargetActor for the content page is the top level target.
   *          For the Multiprocess Browser Toolbox, the parent process target is the top level
   *          target.
   *          At the moment this only impacts the BrowsingContextTarget `reconfigure`
   *          implementation. But for server-side target switching this flag will be exposed
   *          to the client and should be available for all target actor classes. It will be
   *          used to detect target switching. (Bug 1644397)
   */
  initialize: function(
    connection,
    {
      docShell,
      doNotFireFrameUpdates,
      followWindowGlobalLifeCycle,
      isTopLevelTarget,
    }
  ) {
    Actor.prototype.initialize.call(this, connection);

    if (!docShell) {
      throw new Error(
        "A docShell should be provided as constructor argument of BrowsingContextTargetActor"
      );
    }
    this.docShell = docShell;

    this.followWindowGlobalLifeCycle = followWindowGlobalLifeCycle;
    this.doNotFireFrameUpdates = doNotFireFrameUpdates;
    this.isTopLevelTarget = !!isTopLevelTarget;

    // A map of actor names to actor instances provided by extensions.
    this._extraActors = {};
    this._sourcesManager = null;

    // Map of DOM stylesheets to StyleSheetActors
    this._styleSheetActors = new Map();

    this._shouldAddNewGlobalAsDebuggee = this._shouldAddNewGlobalAsDebuggee.bind(
      this
    );

    this.makeDebugger = makeDebugger.bind(null, {
      findDebuggees: () => {
        return this.windows.concat(this.webextensionsContentScriptGlobals);
      },
      shouldAddNewGlobalAsDebuggee: this._shouldAddNewGlobalAsDebuggee,
    });

    // Flag eventually overloaded by sub classes in order to watch new docshells
    // Used by the ParentProcessTargetActor to list all frames in the Browser Toolbox
    this.watchNewDocShells = false;

    this.traits = {
      // Supports frame listing via `listFrames` request and `frameUpdate` events
      // as well as frame switching via `switchToFrame` request
      frames: true,
      // Supports the logInPage request.
      logInPage: true,
      // Supports watchpoints in the server. We need to keep this trait because target
      // actors that don't extend BrowsingContextTargetActor (Worker, ContentProcess, …)
      // might not support watchpoints.
      watchpoints: true,
      // Supports back and forward navigation
      navigation: true,
    };

    this._workerDescriptorActorList = null;
    this._workerDescriptorActorPool = null;
    this._onWorkerDescriptorActorListChanged = this._onWorkerDescriptorActorListChanged.bind(
      this
    );

    TargetActorRegistry.registerTargetActor(this);
  },

  traits: null,

  // Optional console API listener options (e.g. used by the WebExtensionActor to
  // filter console messages by addonID), set to an empty (no options) object by default.
  consoleAPIListenerOptions: {},

  // Optional SourcesManager filter function (e.g. used by the WebExtensionActor to filter
  // sources by addonID), allow all sources by default.
  _allowSource() {
    return true;
  },

  get attached() {
    return !!this._attached;
  },

  /*
   * Return a Debugger instance or create one if there is none yet
   */
  get dbg() {
    if (!this._dbg) {
      this._dbg = this.makeDebugger();
    }
    return this._dbg;
  },

  /**
   * Try to locate the console actor if it exists.
   */
  get _consoleActor() {
    if (this.isDestroyed()) {
      return null;
    }
    const form = this.form();
    return this.conn._getOrCreateActor(form.consoleActor);
  },

  _targetScopedActorPool: null,

  /**
   * An object on which listen for DOMWindowCreated and pageshow events.
   */
  get chromeEventHandler() {
    return getDocShellChromeEventHandler(this.docShell);
  },

  /**
   * Getter for the nsIMessageManager associated to the browsing context.
   */
  get messageManager() {
    try {
      return this.docShell.messageManager;
    } catch (e) {
      // In some cases we can't get a docshell.  We just have no message manager
      // then,
      return null;
    }
  },

  /**
   * Getter for the list of all `docShell`s in the browsing context.
   * @return {Array}
   */
  get docShells() {
    return getChildDocShells(this.docShell);
  },

  /**
   * Getter for the browsing context's current DOM window.
   */
  get window() {
    return this.docShell && this.docShell.domWindow;
  },

  get outerWindowID() {
    if (this.docShell) {
      return this.docShell.outerWindowID;
    }
    return null;
  },

  get browsingContext() {
    return this.docShell?.browsingContext;
  },

  get browsingContextID() {
    return this.browsingContext?.id;
  },

  get browserId() {
    return this.browsingContext?.browserId;
  },

  /**
   * Getter for the WebExtensions ContentScript globals related to the
   * browsing context's current DOM window.
   */
  get webextensionsContentScriptGlobals() {
    // Only retrieve the content scripts globals if the ExtensionContent JSM module
    // has been already loaded (which is true if the WebExtensions internals have already
    // been loaded in the same content process).
    if (Cu.isModuleLoaded(EXTENSION_CONTENT_JSM)) {
      return ExtensionContent.getContentScriptGlobals(this.window);
    }

    return [];
  },

  /**
   * Getter for the list of all content DOM windows in the browsing context.
   * @return {Array}
   */
  get windows() {
    return this.docShells.map(docShell => {
      return docShell.domWindow;
    });
  },

  /**
   * Getter for the original docShell this actor got attached to in the first
   * place.
   * Note that your actor should normally *not* rely on this top level docShell
   * if you want it to show information relative to the iframe that's currently
   * being inspected in the toolbox.
   */
  get originalDocShell() {
    if (!this._originalWindow) {
      return this.docShell;
    }

    return this._originalWindow.docShell;
  },

  /**
   * Getter for the original window this actor got attached to in the first
   * place.
   * Note that your actor should normally *not* rely on this top level window if
   * you want it to show information relative to the iframe that's currently
   * being inspected in the toolbox.
   */
  get originalWindow() {
    return this._originalWindow || this.window;
  },

  /**
   * Getter for the nsIWebProgress for watching this window.
   */
  get webProgress() {
    return this.docShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebProgress);
  },

  /**
   * Getter for the nsIWebNavigation for the target.
   */
  get webNavigation() {
    return this.docShell.QueryInterface(Ci.nsIWebNavigation);
  },

  /**
   * Getter for the browsing context's document.
   */
  get contentDocument() {
    return this.webNavigation.document;
  },

  /**
   * Getter for the browsing context's title.
   */
  get title() {
    return this.contentDocument.contentTitle;
  },

  /**
   * Getter for the browsing context's URL.
   */
  get url() {
    if (this.webNavigation.currentURI) {
      return this.webNavigation.currentURI.spec;
    }
    // Abrupt closing of the browser window may leave callbacks without a
    // currentURI.
    return null;
  },

  get sourcesManager() {
    if (!this._sourcesManager) {
      this._sourcesManager = new SourcesManager(
        this.threadActor,
        this._allowSource
      );
    }
    return this._sourcesManager;
  },

  _createExtraActors() {
    // Always use the same Pool, so existing actor instances
    // (created in createExtraActors) are not lost.
    if (!this._targetScopedActorPool) {
      this._targetScopedActorPool = new LazyPool(this.conn);
    }

    // Walk over target-scoped actor factories and make sure they are all
    // instantiated and added into the Pool.
    return createExtraActors(
      ActorRegistry.targetScopedActorFactories,
      this._targetScopedActorPool,
      this
    );
  },

  form() {
    assert(
      !this.isDestroyed(),
      "form() shouldn't be called on destroyed browser actor."
    );
    assert(this.actorID, "Actor should have an actorID.");

    const response = {
      actor: this.actorID,
      browsingContextID: this.browsingContextID,
      // True for targets created by JSWindowActors, see constructor JSDoc.
      followWindowGlobalLifeCycle: this.followWindowGlobalLifeCycle,
      isTopLevelTarget: this.isTopLevelTarget,
      traits: {
        // @backward-compat { version 64 } Exposes a new trait to help identify
        // BrowsingContextActor's inherited actors from the client side.
        isBrowsingContext: true,
        // @backward-compat { version 87 } Print & color scheme simulations
        // should now be set using reconfigure.
        reconfigureSupportsSimulationFeatures: true,
        // @backward-compat { version 88 } Browsing context targets can compute
        // the isTopLevelTarget flag on the server. Note that not all targets
        // support this, so we might keep this trait until all top level targets
        // can provide this flag consistently from the server.
        supportsTopLevelTargetFlag: true,
        // @backward-compat { version 88 } Added in version 88, will not be
        // available on targets from older servers.
        supportsFollowWindowGlobalLifeCycleFlag: true,
      },
    };

    // We may try to access window while the document is closing, then accessing window
    // throws.
    if (!this.docShell.isBeingDestroyed()) {
      response.title = this.title;
      response.url = this.url;
      response.outerWindowID = this.outerWindowID;
    }

    const actors = this._createExtraActors();
    Object.assign(response, actors);

    // The thread actor is the only actor manually created by the target actor.
    // It is not registered in targetScopedActorFactoriesand therefore needs
    // to be added here manually.
    if (this.threadActor) {
      Object.assign(response, {
        threadActor: this.threadActor.actorID,
      });
    }

    return response;
  },

  /**
   * Called when the actor is removed from the connection.
   */
  destroy() {
    if (this.isDestroyed()) {
      return;
    }
    // Tell the thread actor that the browsing context is closed, so that it may terminate
    // instead of resuming the debuggee script.
    if (this._attached) {
      // TODO: Bug 997119: Remove this coupling with thread actor
      this.threadActor._parentClosed = true;
    }

    this._detach();
    this.docShell = null;
    this._extraActors = null;

    Actor.prototype.destroy.call(this);
    TargetActorRegistry.unregisterTargetActor(this);
    Resources.unwatchAllTargetResources(this);
  },

  /**
   * Return true if the given global is associated with this browsing context and should
   * be added as a debuggee, false otherwise.
   */
  _shouldAddNewGlobalAsDebuggee(wrappedGlobal) {
    // Otherwise, check if it is a WebExtension content script sandbox
    const global = unwrapDebuggerObjectGlobal(wrappedGlobal);
    if (!global) {
      return false;
    }

    // Check if the global is a sdk page-mod sandbox.
    let metadata = {};
    let id = "";
    try {
      id = getInnerId(this.window);
      metadata = Cu.getSandboxMetadata(global);
    } catch (e) {
      // ignore
    }
    if (metadata?.["inner-window-id"] && metadata["inner-window-id"] == id) {
      return true;
    }

    return false;
  },

  /**
   * Does the actual work of attaching to a browsing context.
   */
  _attach() {
    if (this._attached) {
      return;
    }

    // Create a pool for context-lifetime actors.
    this._createThreadActor();

    this._progressListener = new DebuggerProgressListener(this);

    // Save references to the original document we attached to
    this._originalWindow = this.window;

    this._docShellsObserved = false;

    // Ensure replying to attach() request first
    // before notifying about new docshells.
    DevToolsUtils.executeSoon(() => this._watchDocshells());

    this._attached = true;
  },

  _watchDocshells() {
    // If for some unexpected reason, the actor is immediately destroyed,
    // avoid registering leaking observer listener.
    if (this.isDestroyed()) {
      return;
    }

    // In child processes, we watch all docshells living in the process.
    Services.obs.addObserver(this, "webnavigation-create");
    Services.obs.addObserver(this, "webnavigation-destroy");
    this._docShellsObserved = true;

    // We watch for all child docshells under the current document,
    this._progressListener.watch(this.docShell);

    // And list all already existing ones.
    this._updateChildDocShells();
  },

  _unwatchDocshells() {
    if (this._progressListener) {
      this._progressListener.destroy();
      this._progressListener = null;
      this._originalWindow = null;
    }

    // Removes the observers being set in _watchDocshells, but only
    // if _watchDocshells has been called. The target actor may be immediately destroyed
    // and doesn't have time to register them.
    // (Calling removeObserver without having called addObserver throws)
    if (this._docShellsObserved) {
      Services.obs.removeObserver(this, "webnavigation-create");
      Services.obs.removeObserver(this, "webnavigation-destroy");
      this._docShellsObserved = true;
    }
  },

  _unwatchDocShell(docShell) {
    if (this._progressListener) {
      this._progressListener.unwatch(docShell);
    }
  },

  switchToFrame(request) {
    const windowId = request.windowId;
    let win;

    try {
      win = Services.wm.getOuterWindowWithId(windowId);
    } catch (e) {
      // ignore
    }
    if (!win) {
      throw {
        error: "noWindow",
        message: "The related docshell is destroyed or not found",
      };
    } else if (win == this.window) {
      return {};
    }

    // Reply first before changing the document
    DevToolsUtils.executeSoon(() => this._changeTopLevelDocument(win));

    return {};
  },

  listFrames(request) {
    const windows = this._docShellsToWindows(this.docShells);
    return { frames: windows };
  },

  ensureWorkerDescriptorActorList() {
    if (this._workerDescriptorActorList === null) {
      this._workerDescriptorActorList = new WorkerDescriptorActorList(
        this.conn,
        {
          type: Ci.nsIWorkerDebugger.TYPE_DEDICATED,
          window: this.window,
        }
      );
    }
    return this._workerDescriptorActorList;
  },

  pauseWorkersUntilAttach(shouldPause) {
    this.ensureWorkerDescriptorActorList().workerPauser.setPauseMatching(
      shouldPause
    );
  },

  listWorkers(request) {
    if (!this.attached) {
      throw {
        error: "wrongState",
      };
    }

    return this.ensureWorkerDescriptorActorList()
      .getList()
      .then(actors => {
        const pool = new Pool(this.conn, "worker-targets");
        for (const actor of actors) {
          pool.manage(actor);
        }

        // Do not destroy the pool before transfering ownership to the newly created
        // pool, so that we do not accidently destroy actors that are still in use.
        if (this._workerDescriptorActorPool) {
          this._workerDescriptorActorPool.destroy();
        }

        this._workerDescriptorActorPool = pool;
        this._workerDescriptorActorList.onListChanged = this._onWorkerDescriptorActorListChanged;

        return {
          workers: actors,
        };
      });
  },

  logInPage(request) {
    const { text, category, flags } = request;
    const scriptErrorClass = Cc["@mozilla.org/scripterror;1"];
    const scriptError = scriptErrorClass.createInstance(Ci.nsIScriptError);
    scriptError.initWithWindowID(
      text,
      null,
      null,
      0,
      0,
      flags,
      category,
      getInnerId(this.window)
    );
    Services.console.logMessage(scriptError);
    return {};
  },

  _onWorkerDescriptorActorListChanged() {
    this._workerDescriptorActorList.onListChanged = null;
    this.emit("workerListChanged");
  },

  observe(subject, topic, data) {
    // Ignore any event that comes before/after the actor is attached.
    // That typically happens during Firefox shutdown.
    if (!this.attached) {
      return;
    }

    subject.QueryInterface(Ci.nsIDocShell);

    if (topic == "webnavigation-create") {
      this._onDocShellCreated(subject);
    } else if (topic == "webnavigation-destroy") {
      this._onDocShellDestroy(subject);
    }
  },

  _onDocShellCreated(docShell) {
    // (chrome-)webnavigation-create is fired very early during docshell
    // construction. In new root docshells within child processes, involving
    // BrowserChild, this event is from within this call:
    //   https://hg.mozilla.org/mozilla-central/annotate/74d7fb43bb44/dom/ipc/TabChild.cpp#l912
    // whereas the chromeEventHandler (and most likely other stuff) is set
    // later:
    //   https://hg.mozilla.org/mozilla-central/annotate/74d7fb43bb44/dom/ipc/TabChild.cpp#l944
    // So wait a tick before watching it:
    DevToolsUtils.executeSoon(() => {
      // Bug 1142752: sometimes, the docshell appears to be immediately
      // destroyed, bailout early to prevent random exceptions.
      if (docShell.isBeingDestroyed()) {
        return;
      }

      // In child processes, we have new root docshells,
      // let's watch them and all their child docshells.
      if (this._isRootDocShell(docShell) && this.watchNewDocShells) {
        this._progressListener.watch(docShell);
      }
      this._notifyDocShellsUpdate([docShell]);
    });
  },

  _onDocShellDestroy(docShell) {
    // Stop watching this docshell (the unwatch() method will check if we
    // started watching it before).
    this._unwatchDocShell(docShell);

    const webProgress = docShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebProgress);
    this._notifyDocShellDestroy(webProgress);

    if (webProgress.DOMWindow == this._originalWindow) {
      // If the original top level document we connected to is removed,
      // we try to switch to any other top level document
      const rootDocShells = this.docShells.filter(d => {
        return d != this.docShell && this._isRootDocShell(d);
      });
      if (rootDocShells.length > 0) {
        const newRoot = rootDocShells[0];
        this._originalWindow = newRoot.DOMWindow;
        this._changeTopLevelDocument(this._originalWindow);
      } else {
        // If for some reason (typically during Firefox shutdown), the original
        // document is destroyed, and there is no other top level docshell,
        // we detach the actor to unregister all listeners and prevent any
        // exception.
        this.destroy();
      }
      return;
    }

    // If the currently targeted browsing context is destroyed, and we aren't on
    // the top-level document, we have to switch to the top-level one.
    if (
      webProgress.DOMWindow == this.window &&
      this.window != this._originalWindow
    ) {
      this._changeTopLevelDocument(this._originalWindow);
    }
  },

  _isRootDocShell(docShell) {
    // Should report as root docshell:
    //  - New top level window's docshells, when using ParentProcessTargetActor against a
    // process. It allows tracking iframes of the newly opened windows
    // like Browser console or new browser windows.
    //  - MozActivities or window.open frames on B2G, where a new root docshell
    // is spawn in the child process of the app.
    return !docShell.parent;
  },

  _docShellToWindow(docShell) {
    const webProgress = docShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebProgress);
    const window = webProgress.DOMWindow;
    const id = docShell.outerWindowID;
    let parentID = undefined;
    // Ignore the parent of the original document on non-e10s firefox,
    // as we get the xul window as parent and don't care about it.
    // Furthermore, ignore setting parentID when parent window is same as
    // current window in order to deal with front end. e.g. toolbox will be fall
    // into infinite loop due to recursive search with by using parent id.
    if (
      window.parent &&
      window.parent != window &&
      window != this._originalWindow
    ) {
      parentID = window.parent.docShell.outerWindowID;
    }

    return {
      id,
      parentID,
      url: window.location.href,
      title: window.document.title,
    };
  },

  // Convert docShell list to windows objects list being sent to the client
  _docShellsToWindows(docshells) {
    return docshells.map(docShell => this._docShellToWindow(docShell));
  },

  _notifyDocShellsUpdate(docshells) {
    // Only top level target uses frameUpdate in order to update the iframe dropdown.
    // This may eventually be replaced by Target listening and target switching.
    if (this.doNotFireFrameUpdates) {
      return;
    }

    const windows = this._docShellsToWindows(docshells);

    // Do not send the `frameUpdate` event if the windows array is empty.
    if (windows.length == 0) {
      return;
    }

    this.emit("frameUpdate", {
      frames: windows,
    });
  },

  _updateChildDocShells() {
    this._notifyDocShellsUpdate(this.docShells);
  },

  _notifyDocShellDestroy(webProgress) {
    // Only top level target uses frameUpdate in order to update the iframe dropdown.
    // This may eventually be replaced by Target listening and target switching.
    if (this.doNotFireFrameUpdates) {
      return;
    }

    webProgress = webProgress.QueryInterface(Ci.nsIWebProgress);
    const id = webProgress.DOMWindow.docShell.outerWindowID;
    this.emit("frameUpdate", {
      frames: [
        {
          id,
          destroy: true,
        },
      ],
    });
  },

  _notifyDocShellDestroyAll() {
    // Only top level target uses frameUpdate in order to update the iframe dropdown.
    // This may eventually be replaced by Target listening and target switching.
    if (this.doNotFireFrameUpdates) {
      return;
    }

    this.emit("frameUpdate", {
      destroyAll: true,
    });
  },

  /**
   * Creates and manages the thread actor as part of the Browsing Context Target pool.
   * This sets up the content window for being debugged
   */
  _createThreadActor() {
    this.threadActor = new ThreadActor(this, this.window);
    this.manage(this.threadActor);
  },

  /**
   * Exits the current thread actor and removes it from the Browsing Context Target pool.
   * The content window is no longer being debugged after this call.
   */
  _destroyThreadActor() {
    this.threadActor.destroy();
    this.threadActor = null;

    if (this._sourcesManager) {
      this._sourcesManager.destroy();
      this._sourcesManager = null;
    }
  },

  /**
   * Does the actual work of detaching from a browsing context.
   *
   * @returns false if the actor wasn't attached or true of detaching succeeds.
   */
  _detach() {
    if (!this.attached) {
      return false;
    }

    // Check for `docShell` availability, as it can be already gone during
    // Firefox shutdown.
    if (this.docShell) {
      this._unwatchDocShell(this.docShell);
      this._restoreTargetConfiguration();
    }
    this._unwatchDocshells();

    this._destroyThreadActor();

    // Shut down actors that belong to this target's pool.
    this._styleSheetActors.clear();
    if (this._targetScopedActorPool) {
      this._targetScopedActorPool.destroy();
      this._targetScopedActorPool = null;
    }

    // Make sure that no more workerListChanged notifications are sent.
    if (this._workerDescriptorActorList !== null) {
      this._workerDescriptorActorList.destroy();
      this._workerDescriptorActorList = null;
    }

    if (this._workerDescriptorActorPool !== null) {
      this._workerDescriptorActorPool.destroy();
      this._workerDescriptorActorPool = null;
    }

    if (this._dbg) {
      this._dbg.disable();
      this._dbg = null;
    }

    this._attached = false;

    // When the target actor acts as a WindowGlobalTarget, the actor will be destroyed
    // without having to send an RDP event. The parent process will receive a window-global-destroyed
    // and report the target actor as destroyed via the Watcher actor.
    if (this.followWindowGlobalLifeCycle) {
      return true;
    }

    return true;
  },

  // Protocol Request Handlers

  attach(request) {
    if (this.isDestroyed()) {
      throw {
        error: "destroyed",
      };
    }

    this._attach();

    return {
      threadActor: this.threadActor.actorID,
      cacheDisabled: this._getCacheDisabled(),
      javascriptEnabled: this._getJavascriptEnabled(),
      traits: this.traits,
    };
  },

  detach(request) {
    if (!this._detach()) {
      throw {
        error: "wrongState",
      };
    }

    return {};
  },

  /**
   * Bring the browsing context's window to front.
   */
  focus() {
    if (this.window) {
      this.window.focus();
    }
    return {};
  },

  goForward() {
    // Wait a tick so that the response packet can be dispatched before the
    // subsequent navigation event packet.
    Services.tm.dispatchToMainThread(
      DevToolsUtils.makeInfallible(() => {
        // This won't work while the browser is shutting down and we don't really
        // care.
        if (Services.startup.shuttingDown) {
          return;
        }

        this.webNavigation.goForward();
      }, "BrowsingContextTargetActor.prototype.goForward's delayed body")
    );

    return {};
  },

  goBack() {
    // Wait a tick so that the response packet can be dispatched before the
    // subsequent navigation event packet.
    Services.tm.dispatchToMainThread(
      DevToolsUtils.makeInfallible(() => {
        // This won't work while the browser is shutting down and we don't really
        // care.
        if (Services.startup.shuttingDown) {
          return;
        }

        this.webNavigation.goBack();
      }, "BrowsingContextTargetActor.prototype.goBack's delayed body")
    );

    return {};
  },

  /**
   * Reload the page in this browsing context.
   */
  reload(request) {
    const force = request?.options?.force;
    // Wait a tick so that the response packet can be dispatched before the
    // subsequent navigation event packet.
    Services.tm.dispatchToMainThread(
      DevToolsUtils.makeInfallible(() => {
        // This won't work while the browser is shutting down and we don't really
        // care.
        if (Services.startup.shuttingDown) {
          return;
        }

        this.webNavigation.reload(
          force
            ? Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE
            : Ci.nsIWebNavigation.LOAD_FLAGS_NONE
        );
      }, "BrowsingContextTargetActor.prototype.reload's delayed body")
    );
    return {};
  },

  /**
   * Navigate this browsing context to a new location
   */
  navigateTo(request) {
    // Wait a tick so that the response packet can be dispatched before the
    // subsequent navigation event packet.
    Services.tm.dispatchToMainThread(
      DevToolsUtils.makeInfallible(() => {
        this.window.location = request.url;
      }, "BrowsingContextTargetActor.prototype.navigateTo's delayed body:" + request.url)
    );
    return {};
  },

  /**
   * Ensure that CSS error reporting is enabled.
   */
  async ensureCSSErrorReportingEnabled(request) {
    const promises = [];
    for (const docShell of this.docShells) {
      if (docShell.cssErrorReportingEnabled) {
        continue;
      }
      try {
        docShell.cssErrorReportingEnabled = true;
      } catch (e) {
        continue;
      }
      // Ensure docShell.document is available.
      docShell.QueryInterface(Ci.nsIWebNavigation);
      // We don't really want to reparse UA sheets and such, but want to do
      // Shadow DOM / XBL.
      const sheets = InspectorUtils.getAllStyleSheets(
        docShell.document,
        /* documentOnly = */ true
      );
      for (const sheet of sheets) {
        if (InspectorUtils.hasRulesModifiedByCSSOM(sheet)) {
          continue;
        }
        // Reparse the sheet so that we see the existing errors.
        const onStyleSheetParsed = getSheetText(sheet)
          .then(text => {
            InspectorUtils.parseStyleSheet(sheet, text, /* aUpdate = */ false);
          })
          .catch(e => console.error("Error while parsing stylesheet"));
        promises.push(onStyleSheetParsed);
      }
    }
    await Promise.all(promises);
    return {};
  },

  /**
   * For browsing-context targets which can't use the watcher configuration
   * actor (eg webextension targets), the client directly calls `reconfigure`.
   * Once all targets support the watcher, this method can be removed.
   */
  reconfigure(request) {
    const options = request.options || {};
    return this.updateTargetConfiguration(options);
  },

  /**
   * Apply target-specific options.
   *
   * This will be called by the watcher when the DevTools target-configuration
   * is updated, or when a target is created via JSWindowActors.
   */
  updateTargetConfiguration(options = {}) {
    if (!this.docShell) {
      // The browsing context is already closed.
      return;
    }

    if (!this.isTopLevelTarget) {
      // DevTools target options should only apply to the top target and be
      // propagated through the browsing context tree via the platform.
      return;
    }

    // Wait a tick so that the response packet can be dispatched before the
    // subsequent navigation event packet.
    let reload = false;

    if (
      typeof options.javascriptEnabled !== "undefined" &&
      options.javascriptEnabled !== this._getJavascriptEnabled()
    ) {
      this._setJavascriptEnabled(options.javascriptEnabled);
      reload = true;
    }
    if (
      typeof options.cacheDisabled !== "undefined" &&
      options.cacheDisabled !== this._getCacheDisabled()
    ) {
      this._setCacheDisabled(options.cacheDisabled);
    }
    if (
      typeof options.paintFlashing !== "undefined" &&
      options.PaintFlashing !== this._getPaintFlashing()
    ) {
      this._setPaintFlashingEnabled(options.paintFlashing);
    }
    if (typeof options.restoreFocus == "boolean") {
      this._restoreFocus = options.restoreFocus;
    }

    if (reload) {
      this.reload();
    }
  },

  /**
   * Opposite of the updateTargetConfiguration method, that resets document
   * state when closing the toolbox.
   */
  _restoreTargetConfiguration() {
    this._restoreJavascript();
    this._setCacheDisabled(false);
    this._setPaintFlashingEnabled(false);

    if (this._restoreFocus && this.browsingContext?.isActive) {
      this.window.focus();
    }
  },

  /**
   * Disable or enable the cache via docShell.
   */
  _setCacheDisabled(disabled) {
    const enable = Ci.nsIRequest.LOAD_NORMAL;
    const disable = Ci.nsIRequest.LOAD_BYPASS_CACHE;

    this.docShell.defaultLoadFlags = disabled ? disable : enable;
  },

  /**
   * Disable or enable JS via docShell.
   */
  _wasJavascriptEnabled: null,
  _setJavascriptEnabled(allow) {
    if (this._wasJavascriptEnabled === null) {
      this._wasJavascriptEnabled = this.docShell.allowJavascript;
    }
    this.docShell.allowJavascript = allow;
  },

  /**
   * Restore JS state, before the actor modified it.
   */
  _restoreJavascript() {
    if (this._wasJavascriptEnabled !== null) {
      this._setJavascriptEnabled(this._wasJavascriptEnabled);
      this._wasJavascriptEnabled = null;
    }
  },

  /**
   * Return JS allowed status.
   */
  _getJavascriptEnabled() {
    if (!this.docShell) {
      // The browsing context is already closed.
      return null;
    }

    return this.docShell.allowJavascript;
  },

  /**
   * Disable or enable the paint flashing on the target.
   */
  _setPaintFlashingEnabled(enabled) {
    const windowUtils = this.window.windowUtils;
    windowUtils.paintFlashing = enabled;
  },

  /**
   * Return cache allowed status.
   */
  _getCacheDisabled() {
    if (!this.docShell) {
      // The browsing context is already closed.
      return null;
    }

    const disable = Ci.nsIRequest.LOAD_BYPASS_CACHE;
    return this.docShell.defaultLoadFlags === disable;
  },

  /**
   * Return paint flashing status.
   */
  _getPaintFlashing() {
    if (!this.docShell) {
      // The browsing context is already closed.
      return null;
    }

    return this.window.windowUtils.paintFlashing;
  },

  _changeTopLevelDocument(window) {
    // Fake a will-navigate on the previous document
    // to let a chance to unregister it
    this._willNavigate(this.window, window.location.href, null, true);

    this._windowDestroyed(this.window, null, true);

    // Immediately change the window as this window, if in process of unload
    // may already be non working on the next cycle and start throwing
    this._setWindow(window);

    DevToolsUtils.executeSoon(() => {
      // No need to do anything more if the actor is not attached anymore
      // e.g. the client has been closed and the actors destroyed in the meantime.
      if (!this.attached) {
        return;
      }

      // Then fake window-ready and navigate on the given document
      this._windowReady(window, { isFrameSwitching: true });
      DevToolsUtils.executeSoon(() => {
        this._navigate(window, true);
      });
    });
  },

  _setWindow(window) {
    // Here is the very important call where we switch the currently targeted
    // browsing context (it will indirectly update this.window and many other
    // attributes defined from docShell).
    this.docShell = window.docShell;
    this.emit("changed-toplevel-document");
    this.emit("frameUpdate", {
      selected: this.outerWindowID,
    });
  },

  /**
   * Handle location changes, by clearing the previous debuggees and enabling
   * debugging, which may have been disabled temporarily by the
   * DebuggerProgressListener.
   */
  _windowReady(window, { isFrameSwitching, isBFCache } = {}) {
    const isTopLevel = window == this.window;

    // We just reset iframe list on WillNavigate, so we now list all existing
    // frames when we load a new document in the original window
    if (window == this._originalWindow && !isFrameSwitching) {
      this._updateChildDocShells();
    }

    this.emit("window-ready", {
      window: window,
      isTopLevel: isTopLevel,
      isBFCache,
      id: getWindowID(window),
    });
  },

  _windowDestroyed(window, id = null, isFrozen = false) {
    this.emit("window-destroyed", {
      window: window,
      isTopLevel: window == this.window,
      id: id || getWindowID(window),
      isFrozen: isFrozen,
    });
  },

  /**
   * Start notifying server and client about a new document being loaded in the
   * currently targeted browsing context.
   */
  _willNavigate(window, newURI, request, isFrameSwitching = false) {
    let isTopLevel = window == this.window;
    let reset = false;

    if (window == this._originalWindow && !isFrameSwitching) {
      // Clear the iframe list if the original top-level document changes.
      this._notifyDocShellDestroyAll();

      // If the top level document changes and we are targeting an iframe, we
      // need to reset to the upcoming new top level document. But for this
      // will-navigate event, we will dispatch on the old window. (The inspector
      // codebase expect to receive will-navigate for the currently displayed
      // document in order to cleanup the markup view)
      if (this.window != this._originalWindow) {
        reset = true;
        window = this.window;
        isTopLevel = true;
      }
    }

    // will-navigate event needs to be dispatched synchronously, by calling the
    // listeners in the order or registration. This event fires once navigation
    // starts, (all pending user prompts are dealt with), but before the first
    // request starts.
    this.emit("will-navigate", {
      window: window,
      isTopLevel: isTopLevel,
      newURI: newURI,
      request: request,
    });

    // We don't do anything for inner frames here.
    // (we will only update thread actor on window-ready)
    if (!isTopLevel) {
      return;
    }

    // When the actor acts as a WindowGlobalTarget, will-navigate won't fired.
    // Instead we will receive a new top level target with isTargetSwitching=true.
    if (!this.followWindowGlobalLifeCycle) {
      this.emit("tabNavigated", {
        url: newURI,
        nativeConsoleAPI: true,
        state: "start",
        isFrameSwitching: isFrameSwitching,
      });
    }

    if (reset) {
      this._setWindow(this._originalWindow);
    }
  },

  /**
   * Notify server and client about a new document done loading in the current
   * targeted browsing context.
   */
  _navigate(window, isFrameSwitching = false) {
    const isTopLevel = window == this.window;

    // navigate event needs to be dispatched synchronously,
    // by calling the listeners in the order or registration.
    // This event is fired once the document is loaded,
    // after the load event, it's document ready-state is 'complete'.
    this.emit("navigate", {
      window: window,
      isTopLevel: isTopLevel,
    });

    // We don't do anything for inner frames here.
    // (we will only update thread actor on window-ready)
    if (!isTopLevel) {
      return;
    }

    // We may still significate when the document is done loading, via navigate.
    // But as we no longer fire the "will-navigate", may be it is better to find
    // other ways to get to our means.
    // Listening to "navigate" is misleading as the document may already be loaded
    // if we just opened the DevTools. So it is better to use "watch" pattern
    // and instead have the actor either emit immediately resources as they are
    // already available, or later on as the load progresses.
    if (this.followWindowGlobalLifeCycle) {
      return;
    }

    this.emit("tabNavigated", {
      url: this.url,
      title: this.title,
      nativeConsoleAPI: this.hasNativeConsoleAPI(this.window),
      state: "stop",
      isFrameSwitching: isFrameSwitching,
    });
  },

  /**
   * Tells if the window.console object is native or overwritten by script in
   * the page.
   *
   * @param nsIDOMWindow window
   *        The window object you want to check.
   * @return boolean
   *         True if the window.console object is native, or false otherwise.
   */
  hasNativeConsoleAPI(window) {
    let isNative = false;
    try {
      // We are very explicitly examining the "console" property of
      // the non-Xrayed object here.
      const console = window.wrappedJSObject.console;
      isNative = new XPCNativeWrapper(console).IS_NATIVE_CONSOLE;
    } catch (ex) {
      // ignore
    }
    return isNative;
  },

  /**
   * Create or return the StyleSheetActor for a style sheet. This method
   * is here because the Style Editor and Inspector share style sheet actors.
   *
   * @param DOMStyleSheet styleSheet
   *        The style sheet to create an actor for.
   * @return StyleSheetActor actor
   *         The actor for this style sheet.
   *
   */
  createStyleSheetActor(styleSheet) {
    assert(
      !this.isDestroyed(),
      "Target must not be destroyed to create a sheet actor."
    );
    if (this._styleSheetActors.has(styleSheet)) {
      return this._styleSheetActors.get(styleSheet);
    }
    const actor = new StyleSheetActor(styleSheet, this);
    this._styleSheetActors.set(styleSheet, actor);

    this._targetScopedActorPool.manage(actor);
    this.emit("stylesheet-added", actor);

    return actor;
  },

  removeActorByName(name) {
    if (name in this._extraActors) {
      const actor = this._extraActors[name];
      if (this._targetScopedActorPool.has(actor)) {
        this._targetScopedActorPool.removeActor(actor);
      }
      delete this._extraActors[name];
    }
  },
};

exports.browsingContextTargetPrototype = browsingContextTargetPrototype;
exports.BrowsingContextTargetActor = TargetActorMixin(
  Targets.TYPES.FRAME,
  browsingContextTargetSpec,
  browsingContextTargetPrototype
);

/**
 * The DebuggerProgressListener object is an nsIWebProgressListener which
 * handles onStateChange events for the targeted browsing context. If the user
 * tries to navigate away from a paused page, the listener makes sure that the
 * debuggee is resumed before the navigation begins.
 *
 * @param BrowsingContextTargetActor targetActor
 *        The browsing context target actor associated with this listener.
 */
function DebuggerProgressListener(targetActor) {
  this._targetActor = targetActor;
  this._onWindowCreated = this.onWindowCreated.bind(this);
  this._onWindowHidden = this.onWindowHidden.bind(this);

  // Watch for windows destroyed (global observer that will need filtering)
  Services.obs.addObserver(this, "inner-window-destroyed");

  // XXX: for now we maintain the list of windows we know about in this instance
  // so that we can discriminate windows we care about when observing
  // inner-window-destroyed events. Bug 1016952 would remove the need for this.
  this._knownWindowIDs = new Map();

  this._watchedDocShells = new WeakSet();
}

DebuggerProgressListener.prototype = {
  QueryInterface: ChromeUtils.generateQI([
    "nsIWebProgressListener",
    "nsISupportsWeakReference",
  ]),

  destroy() {
    Services.obs.removeObserver(this, "inner-window-destroyed");
    this._knownWindowIDs.clear();
    this._knownWindowIDs = null;
  },

  watch(docShell) {
    // Add the docshell to the watched set. We're actually adding the window,
    // because docShell objects are not wrappercached and would be rejected
    // by the WeakSet.
    const docShellWindow = docShell.domWindow;
    this._watchedDocShells.add(docShellWindow);

    const webProgress = docShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(
      this,
      Ci.nsIWebProgress.NOTIFY_STATE_WINDOW |
        Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT
    );

    const handler = getDocShellChromeEventHandler(docShell);
    handler.addEventListener("DOMWindowCreated", this._onWindowCreated, true);
    handler.addEventListener("pageshow", this._onWindowCreated, true);
    handler.addEventListener("pagehide", this._onWindowHidden, true);

    // Dispatch the _windowReady event on the targetActor for pre-existing windows
    for (const win of this._getWindowsInDocShell(docShell)) {
      this._targetActor._windowReady(win);
      this._knownWindowIDs.set(getWindowID(win), win);
    }

    // The `watchedByDevTools` enables gecko behavior tied to this flag, such as:
    //  - reporting the contents of HTML loaded in the docshells,
    //  - or capturing stacks for the network monitor.
    //
    // This attribute may already have been toggled by a parent BrowsingContext.
    // Typically the parent process or tab target. Both are top level BrowsingContext.
    if (docShell.browsingContext.top == docShell.browsingContext) {
      docShell.browsingContext.watchedByDevTools = true;
    }
  },

  unwatch(docShell) {
    const docShellWindow = docShell.domWindow;
    if (!this._watchedDocShells.has(docShellWindow)) {
      return;
    }
    this._watchedDocShells.delete(docShellWindow);

    const webProgress = docShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebProgress);
    // During process shutdown, the docshell may already be cleaned up and throw
    try {
      webProgress.removeProgressListener(this);
    } catch (e) {
      // ignore
    }

    const handler = getDocShellChromeEventHandler(docShell);
    handler.removeEventListener(
      "DOMWindowCreated",
      this._onWindowCreated,
      true
    );
    handler.removeEventListener("pageshow", this._onWindowCreated, true);
    handler.removeEventListener("pagehide", this._onWindowHidden, true);

    for (const win of this._getWindowsInDocShell(docShell)) {
      this._knownWindowIDs.delete(getWindowID(win));
    }

    // We can only toggle this attribute on top level BrowsingContext,
    // this will be propagated over the whole tree of BC.
    // So we only need to set it from Parent Process Target
    // and Tab Target. Tab's BrowsingContext are actually considered as top level BC.
    if (docShell.browsingContext.top == docShell.browsingContext) {
      docShell.browsingContext.watchedByDevTools = false;
    }
  },

  _getWindowsInDocShell(docShell) {
    return getChildDocShells(docShell).map(d => {
      return d.domWindow;
    });
  },

  onWindowCreated: DevToolsUtils.makeInfallible(function(evt) {
    if (!this._targetActor.attached) {
      return;
    }

    // If we're in a frame swap (which occurs when toggling RDM, for example), then we can
    // ignore this event, as the window never really went anywhere for our purposes.
    if (evt.inFrameSwap) {
      return;
    }

    const window = evt.target.defaultView;
    if (!window) {
      // Some old UIs might emit unrelated events called pageshow/pagehide on
      // elements which are not documents. Bail in this case. See Bug 1669666.
      return;
    }

    const innerID = getWindowID(window);

    // This handler is called for two events: "DOMWindowCreated" and "pageshow".
    // Bail out if we already processed this window.
    if (this._knownWindowIDs.has(innerID)) {
      return;
    }
    this._knownWindowIDs.set(innerID, window);

    // For a regular page navigation, "DOMWindowCreated" is fired before
    // "pageshow". If the current event is "pageshow" but we have not processed
    // the window yet, it means this is a BF cache navigation. In theory,
    // `event.persisted` should be set for BF cache navigation events, but it is
    // not always available, so we fallback on checking if "pageshow" is the
    // first event received for a given window (see Bug 1378133).
    const isBFCache = evt.type == "pageshow";

    this._targetActor._windowReady(window, { isBFCache });
  }, "DebuggerProgressListener.prototype.onWindowCreated"),

  onWindowHidden: DevToolsUtils.makeInfallible(function(evt) {
    if (!this._targetActor.attached) {
      return;
    }

    // If we're in a frame swap (which occurs when toggling RDM, for example), then we can
    // ignore this event, as the window isn't really going anywhere for our purposes.
    if (evt.inFrameSwap) {
      return;
    }

    // Only act as if the window has been destroyed if the 'pagehide' event
    // was sent for a persisted window (persisted is set when the page is put
    // and frozen in the bfcache). If the page isn't persisted, the observer's
    // inner-window-destroyed event will handle it.
    if (!evt.persisted) {
      return;
    }

    const window = evt.target.defaultView;
    if (!window) {
      // Some old UIs might emit unrelated events called pageshow/pagehide on
      // elements which are not documents. Bail in this case. See Bug 1669666.
      return;
    }

    this._targetActor._windowDestroyed(window, null, true);
    this._knownWindowIDs.delete(getWindowID(window));
  }, "DebuggerProgressListener.prototype.onWindowHidden"),

  observe: DevToolsUtils.makeInfallible(function(subject, topic) {
    if (!this._targetActor.attached) {
      return;
    }

    // Because this observer will be called for all inner-window-destroyed in
    // the application, we need to filter out events for windows we are not
    // watching
    const innerID = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
    const window = this._knownWindowIDs.get(innerID);
    if (window) {
      this._knownWindowIDs.delete(innerID);
      this._targetActor._windowDestroyed(window, innerID);
    }

    // Bug 1598364: when debugging browser.xhtml from the Browser Toolbox
    // the DOMWindowCreated/pageshow/pagehide event listeners have to be
    // re-registered against the next document when we reload browser.html
    // (or navigate to another doc).
    // That's because we registered the listener on docShell.domWindow as
    // top level windows don't have a chromeEventHandler.
    if (
      this._watchedDocShells.has(window) &&
      !window.docShell.chromeEventHandler
    ) {
      // First cleanup all the existing listeners
      this.unwatch(window.docShell);
      // Re-register new ones. The docShell is already referencing the new document.
      this.watch(window.docShell);
    }
  }, "DebuggerProgressListener.prototype.observe"),

  onStateChange: DevToolsUtils.makeInfallible(function(
    progress,
    request,
    flag,
    status
  ) {
    if (!this._targetActor.attached) {
      return;
    }
    progress.QueryInterface(Ci.nsIDocShell);
    if (progress.isBeingDestroyed()) {
      return;
    }

    const isStart = flag & Ci.nsIWebProgressListener.STATE_START;
    const isStop = flag & Ci.nsIWebProgressListener.STATE_STOP;
    const isDocument = flag & Ci.nsIWebProgressListener.STATE_IS_DOCUMENT;
    const isWindow = flag & Ci.nsIWebProgressListener.STATE_IS_WINDOW;

    // Catch any iframe location change
    if (isDocument && isStop) {
      // Watch document stop to ensure having the new iframe url.
      this._targetActor._notifyDocShellsUpdate([progress]);
    }

    const window = progress.DOMWindow;
    if (isDocument && isStart) {
      // One of the earliest events that tells us a new URI
      // is being loaded in this window.
      const newURI = request instanceof Ci.nsIChannel ? request.URI.spec : null;
      this._targetActor._willNavigate(window, newURI, request);
    }
    if (isWindow && isStop) {
      // Don't dispatch "navigate" event just yet when there is a redirect to
      // about:neterror page.
      // Navigating to about:neterror will make `status` be something else than NS_OK.
      // But for some error like NS_BINDING_ABORTED we don't want to emit any `navigate`
      // event as the page load has been cancelled and the related page document is going
      // to be a dead wrapper.
      if (
        request.status != Cr.NS_OK &&
        request.status != Cr.NS_BINDING_ABORTED
      ) {
        // Instead, listen for DOMContentLoaded as about:neterror is loaded
        // with LOAD_BACKGROUND flags and never dispatches load event.
        // That may be the same reason why there is no onStateChange event
        // for about:neterror loads.
        const handler = getDocShellChromeEventHandler(progress);
        const onLoad = evt => {
          // Ignore events from iframes
          if (evt.target === window.document) {
            handler.removeEventListener("DOMContentLoaded", onLoad, true);
            this._targetActor._navigate(window);
          }
        };
        handler.addEventListener("DOMContentLoaded", onLoad, true);
      } else {
        // Somewhat equivalent of load event.
        // (window.document.readyState == complete)
        this._targetActor._navigate(window);
      }
    }
  },
  "DebuggerProgressListener.prototype.onStateChange"),
};
