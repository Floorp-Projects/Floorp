/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global XPCNativeWrapper */

/*
 * BrowsingContextTargetActor is an abstract class used by target actors that hold
 * documents, such as frames, chrome windows, etc.
 *
 * This class is extended by FrameTargetActor, ParentProcessTargetActor, and
 * ChromeWindowTargetActor.
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
var {
  ActorPool, createExtraActors, appendExtraActors
} = require("devtools/server/actors/common");
var { DebuggerServer } = require("devtools/server/main");
var DevToolsUtils = require("devtools/shared/DevToolsUtils");
var { assert } = DevToolsUtils;
var { TabSources } = require("devtools/server/actors/utils/TabSources");
var makeDebugger = require("devtools/server/actors/utils/make-debugger");
const InspectorUtils = require("InspectorUtils");

const EXTENSION_CONTENT_JSM = "resource://gre/modules/ExtensionContent.jsm";

const { LocalizationHelper } = require("devtools/shared/l10n");
const STRINGS_URI = "devtools/shared/locales/browsing-context.properties";
const L10N = new LocalizationHelper(STRINGS_URI);

const { ActorClassWithSpec, Actor } = require("devtools/shared/protocol");
const { browsingContextTargetSpec } = require("devtools/shared/specs/targets/browsing-context");

loader.lazyRequireGetter(this, "ThreadActor", "devtools/server/actors/thread", true);
loader.lazyRequireGetter(this, "unwrapDebuggerObjectGlobal", "devtools/server/actors/thread", true);
loader.lazyRequireGetter(this, "WorkerTargetActorList", "devtools/server/actors/worker/worker-list", true);
loader.lazyImporter(this, "ExtensionContent", EXTENSION_CONTENT_JSM);

loader.lazyRequireGetter(this, "StyleSheetActor", "devtools/server/actors/stylesheets", true);
loader.lazyRequireGetter(this, "getSheetText", "devtools/server/actors/stylesheets", true);

function getWindowID(window) {
  return window.QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIDOMWindowUtils)
               .currentInnerWindowID;
}

function getDocShellChromeEventHandler(docShell) {
  let handler = docShell.chromeEventHandler;
  if (!handler) {
    try {
      // Toplevel xul window's docshell doesn't have chromeEventHandler
      // attribute. The chrome event handler is just the global window object.
      handler = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIDOMWindow);
    } catch (e) {
      // ignore
    }
  }
  return handler;
}

function getChildDocShells(parentDocShell) {
  const docShellsEnum = parentDocShell.getDocShellEnumerator(
    Ci.nsIDocShellTreeItem.typeAll,
    Ci.nsIDocShell.ENUMERATE_FORWARDS
  );

  const docShells = [];
  while (docShellsEnum.hasMoreElements()) {
    const docShell = docShellsEnum.getNext();
    docShell.QueryInterface(Ci.nsIInterfaceRequestor)
            .getInterface(Ci.nsIWebProgress);
    docShells.push(docShell);
  }
  return docShells;
}

exports.getChildDocShells = getChildDocShells;

/**
 * Browser-specific actors.
 */

function getInnerId(window) {
  return window.QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID;
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
   * via `DebuggerServer.registerModule` and manage their lifetimes. In addition, this
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
   * @param connection DebuggerServerConnection
   *        The conection to the client.
   */
  initialize: function(connection) {
    Actor.prototype.initialize.call(this, connection);

    // A map of actor names to actor instances provided by extensions.
    this._extraActors = {};
    this._exited = false;
    this._sources = null;

    // Map of DOM stylesheets to StyleSheetActors
    this._styleSheetActors = new Map();

    this._shouldAddNewGlobalAsDebuggee =
      this._shouldAddNewGlobalAsDebuggee.bind(this);

    this.makeDebugger = makeDebugger.bind(null, {
      findDebuggees: () => {
        return this.windows.concat(this.webextensionsContentScriptGlobals);
      },
      shouldAddNewGlobalAsDebuggee: this._shouldAddNewGlobalAsDebuggee
    });

    // Flag eventually overloaded by sub classes in order to watch new docshells
    // Used by the ParentProcessTargetActor to list all frames in the Browser Toolbox
    this.listenForNewDocShells = false;

    this.traits = {
      reconfigure: true,
      // Supports frame listing via `listFrames` request and `frameUpdate` events
      // as well as frame switching via `switchToFrame` request
      frames: true,
      // Do not require to send reconfigure request to reset the document state
      // to what it was before using the actor
      noTabReconfigureOnClose: true,
      // Supports the logInPage request.
      logInPage: true,
    };

    this._workerTargetActorList = null;
    this._workerTargetActorPool = null;
    this._onWorkerTargetActorListChanged =
      this._onWorkerTargetActorListChanged.bind(this);
  },

  traits: null,

  // Optional console API listener options (e.g. used by the WebExtensionActor to
  // filter console messages by addonID), set to an empty (no options) object by default.
  consoleAPIListenerOptions: {},

  // Optional TabSources filter function (e.g. used by the WebExtensionActor to filter
  // sources by addonID), allow all sources by default.
  _allowSource() {
    return true;
  },

  get exited() {
    return this._exited;
  },

  get attached() {
    return !!this._attached;
  },

  /**
   * Try to locate the console actor if it exists.
   */
  get _consoleActor() {
    if (this.exited) {
      return null;
    }
    const form = this.form();
    return this.conn._getOrCreateActor(form.consoleActor);
  },

  _targetScopedActorPool: null,

  /**
   * A constant prefix that will be used to form the actor ID by the server.
   */
  typeName: "browsingContextTarget",

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
      return this.docShell
        .QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIContentFrameMessageManager);
    } catch (e) {
      return null;
    }
  },

  /**
   * Getter for the browsing context's `docShell`.
   */
  get docShell() {
    throw new Error("`docShell` getter should be overridden by a subclass of " +
                    "`BrowsingContextTargetActor`");
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
    // On xpcshell, there is no document
    if (this.docShell) {
      return this.docShell
        .QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIDOMWindow);
    }
    return null;
  },

  get outerWindowID() {
    if (this.window) {
      return this.window.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIDOMWindowUtils)
                        .outerWindowID;
    }
    return null;
  },

  /**
   * Getter for the WebExtensions ContentScript globals related to the
   * browsing context's current DOM window.
   */
  get webextensionsContentScriptGlobals() {
    // Ignore xpcshell runtime which spawns target actors without a window
    // and only retrieve the content scripts globals if the ExtensionContent JSM module
    // has been already loaded (which is true if the WebExtensions internals have already
    // been loaded in the same content process).
    if (this.window && Cu.isModuleLoaded(EXTENSION_CONTENT_JSM)) {
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
      return docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIDOMWindow);
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

    return this._originalWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                               .getInterface(Ci.nsIWebNavigation)
                               .QueryInterface(Ci.nsIDocShell);
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
    return this.docShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebNavigation);
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

  get sources() {
    if (!this._sources) {
      this._sources = new TabSources(this.threadActor, this._allowSource);
    }
    return this._sources;
  },

  form() {
    assert(!this.exited,
               "form() shouldn't be called on exited browser actor.");
    assert(this.actorID,
               "Actor should have an actorID.");

    const response = {
      actor: this.actorID
    };

    // We may try to access window while the document is closing, then
    // accessing window throws. Also on xpcshell we are using this actor even if
    // there is no valid document.
    if (this.docShell && !this.docShell.isBeingDestroyed()) {
      response.title = this.title;
      response.url = this.url;
      response.outerWindowID = this.outerWindowID;
    }

    // Always use the same ActorPool, so existing actor instances
    // (created in createExtraActors) are not lost.
    if (!this._targetScopedActorPool) {
      this._targetScopedActorPool = new ActorPool(this.conn);
      this.conn.addActorPool(this._targetScopedActorPool);
    }

    // Walk over target-scoped actor factories and make sure they are all
    // instantiated and added into the ActorPool.
    this._createExtraActors(DebuggerServer.targetScopedActorFactories,
      this._targetScopedActorPool);

    this._appendExtraActors(response);
    return response;
  },

  /**
   * Called when the actor is removed from the connection.
   */
  destroy() {
    Actor.prototype.destroy.call(this);
    this.exit();
  },

  /**
   * Called by the root actor when the underlying browsing context is closed.
   */
  exit() {
    if (this.exited) {
      return;
    }

    // Tell the thread actor that the browsing context is closed, so that it may terminate
    // instead of resuming the debuggee script.
    if (this._attached) {
      // TODO: Bug 997119: Remove this coupling with thread actor
      this.threadActor._parentClosed = true;
    }

    this._detach();

    Object.defineProperty(this, "docShell", {
      value: null,
      configurable: true
    });

    this._extraActors = null;

    this._exited = true;
  },

  /**
   * Return true if the given global is associated with this browsing context and should
   * be added as a debuggee, false otherwise.
   */
  _shouldAddNewGlobalAsDebuggee(wrappedGlobal) {
    if (wrappedGlobal.hostAnnotations &&
        wrappedGlobal.hostAnnotations.type == "document" &&
        wrappedGlobal.hostAnnotations.element === this.window) {
      return true;
    }

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
    if (metadata
        && metadata["inner-window-id"]
        && metadata["inner-window-id"] == id) {
      return true;
    }

    return false;
  },

  /* Support for DebuggerServer.addTargetScopedActor. */
  _createExtraActors: createExtraActors,
  _appendExtraActors: appendExtraActors,

  /**
   * Does the actual work of attaching to a browsing context.
   */
  _attach() {
    if (this._attached) {
      return;
    }

    // Create a pool for context-lifetime actors.
    this._createThreadActor();

    // on xpcshell, there is no document
    if (this.window) {
      this._progressListener = new DebuggerProgressListener(this);

      // Save references to the original document we attached to
      this._originalWindow = this.window;

      // Ensure replying to attach() request first
      // before notifying about new docshells.
      DevToolsUtils.executeSoon(() => this._watchDocshells());
    }

    this._attached = true;
  },

  _watchDocshells() {
    // In child processes, we watch all docshells living in the process.
    if (this.listenForNewDocShells) {
      Services.obs.addObserver(this, "webnavigation-create");
    }
    Services.obs.addObserver(this, "webnavigation-destroy");

    // We watch for all child docshells under the current document,
    this._progressListener.watch(this.docShell);

    // And list all already existing ones.
    this._updateChildDocShells();
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
      return { error: "noWindow",
               message: "The related docshell is destroyed or not found" };
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

  listWorkers(request) {
    if (!this.attached) {
      return { error: "wrongState" };
    }

    if (this._workerTargetActorList === null) {
      this._workerTargetActorList = new WorkerTargetActorList(this.conn, {
        type: Ci.nsIWorkerDebugger.TYPE_DEDICATED,
        window: this.window
      });
    }

    return this._workerTargetActorList.getList().then((actors) => {
      const pool = new ActorPool(this.conn);
      for (const actor of actors) {
        pool.addActor(actor);
      }

      this.conn.removeActorPool(this._workerTargetActorPool);
      this._workerTargetActorPool = pool;
      this.conn.addActorPool(this._workerTargetActorPool);

      this._workerTargetActorList.onListChanged = this._onWorkerTargetActorListChanged;

      return {
        "from": this.actorID,
        "workers": actors.map((actor) => actor.form())
      };
    });
  },

  logInPage(request) {
    const {text, category, flags} = request;
    const scriptErrorClass = Cc["@mozilla.org/scripterror;1"];
    const scriptError = scriptErrorClass.createInstance(Ci.nsIScriptError);
    scriptError.initWithWindowID(text, null, null, 0, 0, flags,
                                 category, getInnerId(this.window));
    Services.console.logMessage(scriptError);
    return {};
  },

  _onWorkerTargetActorListChanged() {
    this._workerTargetActorList.onListChanged = null;
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
    // TabChild, this event is from within this call:
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
      if (this._isRootDocShell(docShell)) {
        this._progressListener.watch(docShell);
      }
      this._notifyDocShellsUpdate([docShell]);
    });
  },

  _onDocShellDestroy(docShell) {
    // Stop watching this docshell (the unwatch() method will check if we
    // started watching it before).
    this._unwatchDocShell(docShell);

    const webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebProgress);
    this._notifyDocShellDestroy(webProgress);

    if (webProgress.DOMWindow == this._originalWindow) {
      // If the original top level document we connected to is removed,
      // we try to switch to any other top level document
      const rootDocShells = this.docShells
                              .filter(d => {
                                return d != this.docShell &&
                                       this._isRootDocShell(d);
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
        this.exit();
      }
      return;
    }

    // If the currently targeted browsing context is destroyed, and we aren't on
    // the top-level document, we have to switch to the top-level one.
    if (webProgress.DOMWindow == this.window &&
        this.window != this._originalWindow) {
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
    const webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebProgress);
    const window = webProgress.DOMWindow;
    const id = window.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIDOMWindowUtils)
                   .outerWindowID;
    let parentID = undefined;
    // Ignore the parent of the original document on non-e10s firefox,
    // as we get the xul window as parent and don't care about it.
    // Furthermore, ignore setting parentID when parent window is same as
    // current window in order to deal with front end. e.g. toolbox will be fall
    // into infinite loop due to recursive search with by using parent id.
    if (window.parent && window.parent != window && window != this._originalWindow) {
      parentID = window.parent
                       .QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDOMWindowUtils)
                       .outerWindowID;
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
    const windows = this._docShellsToWindows(docshells);

    // Do not send the `frameUpdate` event if the windows array is empty.
    if (windows.length == 0) {
      return;
    }

    this.emit("frameUpdate", {
      frames: windows
    });
  },

  _updateChildDocShells() {
    this._notifyDocShellsUpdate(this.docShells);
  },

  _notifyDocShellDestroy(webProgress) {
    webProgress = webProgress.QueryInterface(Ci.nsIWebProgress);
    const id = webProgress.DOMWindow
                        .QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIDOMWindowUtils)
                        .outerWindowID;
    this.emit("frameUpdate", {
      frames: [{
        id,
        destroy: true
      }]
    });
  },

  _notifyDocShellDestroyAll() {
    this.emit("frameUpdate", {
      destroyAll: true
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
    this.threadActor.exit();
    this.threadActor = null;
    this._sources = null;
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
      this._restoreDocumentSettings();
    }
    if (this._progressListener) {
      this._progressListener.destroy();
      this._progressListener = null;
      this._originalWindow = null;

      // Removes the observers being set in _watchDocShells
      if (this.listenForNewDocShells) {
        Services.obs.removeObserver(this, "webnavigation-create");
      }
      Services.obs.removeObserver(this, "webnavigation-destroy");
    }

    this._destroyThreadActor();

    // Shut down actors that belong to this target's pool.
    this._styleSheetActors.clear();
    if (this._targetScopedActorPool) {
      this.conn.removeActorPool(this._targetScopedActorPool);
      this._targetScopedActorPool = null;
    }

    // Make sure that no more workerListChanged notifications are sent.
    if (this._workerTargetActorList !== null) {
      this._workerTargetActorList.onListChanged = null;
      this._workerTargetActorList = null;
    }

    if (this._workerTargetActorPool !== null) {
      this.conn.removeActorPool(this._workerTargetActorPool);
      this._workerTargetActorPool = null;
    }

    this._attached = false;

    this.emit("tabDetached");

    return true;
  },

  // Protocol Request Handlers

  attach(request) {
    if (this.exited) {
      return { type: "exited" };
    }

    this._attach();

    return {
      type: "tabAttached",
      threadActor: this.threadActor.actorID,
      cacheDisabled: this._getCacheDisabled(),
      javascriptEnabled: this._getJavascriptEnabled(),
      traits: this.traits,
    };
  },

  detach(request) {
    if (!this._detach()) {
      return { error: "wrongState" };
    }

    return { type: "detached" };
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

  /**
   * Reload the page in this browsing context.
   */
  reload(request) {
    const force = request && request.options && request.options.force;
    // Wait a tick so that the response packet can be dispatched before the
    // subsequent navigation event packet.
    Services.tm.dispatchToMainThread(DevToolsUtils.makeInfallible(() => {
      // This won't work while the browser is shutting down and we don't really
      // care.
      if (Services.startup.shuttingDown) {
        return;
      }
      this.webNavigation.reload(force ?
        Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE :
        Ci.nsIWebNavigation.LOAD_FLAGS_NONE);
    }, "BrowsingContextTargetActor.prototype.reload's delayed body"));
    return {};
  },

  /**
   * Navigate this browsing context to a new location
   */
  navigateTo(request) {
    // Wait a tick so that the response packet can be dispatched before the
    // subsequent navigation event packet.
    Services.tm.dispatchToMainThread(DevToolsUtils.makeInfallible(() => {
      this.window.location = request.url;
    }, "BrowsingContextTargetActor.prototype.navigateTo's delayed body"));
    return {};
  },

  /**
   * Reconfigure options.
   */
  reconfigure(request) {
    const options = request.options || {};

    if (!this.docShell) {
      // The browsing context is already closed.
      return {};
    }
    this._toggleDevToolsSettings(options);

    return {};
  },

  /**
   * Ensure that CSS error reporting is enabled.
   */
  ensureCSSErrorReportingEnabled(request) {
    for (const docShell of this.docShells) {
      if (docShell.cssErrorReportingEnabled) {
        continue;
      }
      try {
        docShell.cssErrorReportingEnabled = true;
      } catch (e) {
        continue;
      }
      // We don't really want to reparse UA sheets and such, but want to do
      // Shadow DOM / XBL.
      const sheets =
        InspectorUtils.getAllStyleSheets(docShell.document, /* documentOnly = */ true);
      const promises = [];
      for (const sheet of sheets) {
        if (InspectorUtils.hasRulesModifiedByCSSOM(sheet)) {
          continue;
        }
        // Reparse the sheet so that we see the existing errors.
        promises.push(getSheetText(sheet, this._consoleActor).then(text => {
          InspectorUtils.parseStyleSheet(sheet, text, /* aUpdate = */ false);
        }));
      }

      Promise.all(promises).then(() => {
        this.logInPage({
          text: L10N.getStr("cssSheetsReparsedWarning"),
          category: "CSS Parser",
          flags: Ci.nsIScriptError.warningFlag,
        });
      });
    }

    return {};
  },

  /**
   * Handle logic to enable/disable JS/cache/Service Worker testing.
   */
  _toggleDevToolsSettings(options) {
    // Wait a tick so that the response packet can be dispatched before the
    // subsequent navigation event packet.
    let reload = false;

    if (typeof options.javascriptEnabled !== "undefined" &&
        options.javascriptEnabled !== this._getJavascriptEnabled()) {
      this._setJavascriptEnabled(options.javascriptEnabled);
      reload = true;
    }
    if (typeof options.cacheDisabled !== "undefined" &&
        options.cacheDisabled !== this._getCacheDisabled()) {
      this._setCacheDisabled(options.cacheDisabled);
    }
    if ((typeof options.serviceWorkersTestingEnabled !== "undefined") &&
        (options.serviceWorkersTestingEnabled !==
         this._getServiceWorkersTestingEnabled())) {
      this._setServiceWorkersTestingEnabled(
        options.serviceWorkersTestingEnabled
      );
    }

    // Reload if:
    //  - there's an explicit `performReload` flag and it's true
    //  - there's no `performReload` flag, but it makes sense to do so
    const hasExplicitReloadFlag = "performReload" in options;
    if ((hasExplicitReloadFlag && options.performReload) ||
       (!hasExplicitReloadFlag && reload)) {
      this.reload();
    }
  },

  /**
   * Opposite of the _toggleDevToolsSettings method, that reset document state
   * when closing the toolbox.
   */
  _restoreDocumentSettings() {
    this._restoreJavascript();
    this._setCacheDisabled(false);
    this._setServiceWorkersTestingEnabled(false);
  },

  /**
   * Disable or enable the cache via docShell.
   */
  _setCacheDisabled(disabled) {
    const enable = Ci.nsIRequest.LOAD_NORMAL;
    const disable = Ci.nsIRequest.LOAD_BYPASS_CACHE |
                  Ci.nsIRequest.INHIBIT_CACHING;

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
   * Disable or enable the service workers testing features.
   */
  _setServiceWorkersTestingEnabled(enabled) {
    const windowUtils = this.window.QueryInterface(Ci.nsIInterfaceRequestor)
                                 .getInterface(Ci.nsIDOMWindowUtils);
    windowUtils.serviceWorkersTestingEnabled = enabled;
  },

  /**
   * Return cache allowed status.
   */
  _getCacheDisabled() {
    if (!this.docShell) {
      // The browsing context is already closed.
      return null;
    }

    const disable = Ci.nsIRequest.LOAD_BYPASS_CACHE |
                  Ci.nsIRequest.INHIBIT_CACHING;
    return this.docShell.defaultLoadFlags === disable;
  },

  /**
   * Return service workers testing allowed status.
   */
  _getServiceWorkersTestingEnabled() {
    if (!this.docShell) {
      // The browsing context is already closed.
      return null;
    }

    const windowUtils = this.window.QueryInterface(Ci.nsIInterfaceRequestor)
                                 .getInterface(Ci.nsIDOMWindowUtils);
    return windowUtils.serviceWorkersTestingEnabled;
  },

  /**
   * Prepare to enter a nested event loop by disabling debuggee events.
   */
  preNest() {
    if (!this.window) {
      // The browsing context is already closed.
      return;
    }
    const windowUtils = this.window
                          .QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIDOMWindowUtils);
    windowUtils.suppressEventHandling(true);
    windowUtils.suspendTimeouts();
  },

  /**
   * Prepare to exit a nested event loop by enabling debuggee events.
   */
  postNest(nestData) {
    if (!this.window) {
      // The browsing context is already closed.
      return;
    }
    const windowUtils = this.window
                          .QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIDOMWindowUtils);
    windowUtils.resumeTimeouts();
    windowUtils.suppressEventHandling(false);
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
      this._windowReady(window, true);
      DevToolsUtils.executeSoon(() => {
        this._navigate(window, true);
      });
    });
  },

  _setWindow(window) {
    const docShell = window.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIWebNavigation)
                         .QueryInterface(Ci.nsIDocShell);
    // Here is the very important call where we switch the currently targeted
    // browsing context (it will indirectly update this.window and many other
    // attributes defined from docShell).
    Object.defineProperty(this, "docShell", {
      value: docShell,
      enumerable: true,
      configurable: true
    });
    this.emit("changed-toplevel-document");
    this.emit("frameUpdate", {
      selected: this.outerWindowID
    });
  },

  /**
   * Handle location changes, by clearing the previous debuggees and enabling
   * debugging, which may have been disabled temporarily by the
   * DebuggerProgressListener.
   */
  _windowReady(window, isFrameSwitching = false) {
    const isTopLevel = window == this.window;

    // We just reset iframe list on WillNavigate, so we now list all existing
    // frames when we load a new document in the original window
    if (window == this._originalWindow && !isFrameSwitching) {
      this._updateChildDocShells();
    }

    this.emit("window-ready", {
      window: window,
      isTopLevel: isTopLevel,
      id: getWindowID(window)
    });

    // TODO bug 997119: move that code to ThreadActor by listening to
    // window-ready
    const threadActor = this.threadActor;
    if (isTopLevel && threadActor.state != "detached") {
      this.sources.reset({ sourceMaps: true });
      threadActor.clearDebuggees();
      threadActor.dbg.enabled = true;
      threadActor.maybePauseOnExceptions();
      // Update the global no matter if the debugger is on or off,
      // otherwise the global will be wrong when enabled later.
      threadActor.global = window;
    }

    // Refresh the debuggee list when a new window object appears (top window or
    // iframe).
    if (threadActor.attached) {
      threadActor.dbg.addDebuggees();
    }
  },

  _windowDestroyed(window, id = null, isFrozen = false) {
    this.emit("window-destroyed", {
      window: window,
      isTopLevel: window == this.window,
      id: id || getWindowID(window),
      isFrozen: isFrozen
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
      request: request
    });

    // We don't do anything for inner frames here.
    // (we will only update thread actor on window-ready)
    if (!isTopLevel) {
      return;
    }

    // Proceed normally only if the debuggee is not paused.
    // TODO bug 997119: move that code to ThreadActor by listening to
    // will-navigate
    const threadActor = this.threadActor;
    if (threadActor.state == "paused") {
      this.conn.send(
        threadActor.unsafeSynchronize(Promise.resolve(threadActor.onResume())));
      threadActor.dbg.enabled = false;
    }
    threadActor.disableAllBreakpoints();

    this.emit("tabNavigated", {
      url: newURI,
      nativeConsoleAPI: true,
      state: "start",
      isFrameSwitching: isFrameSwitching
    });

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
      isTopLevel: isTopLevel
    });

    // We don't do anything for inner frames here.
    // (we will only update thread actor on window-ready)
    if (!isTopLevel) {
      return;
    }

    // TODO bug 997119: move that code to ThreadActor by listening to navigate
    const threadActor = this.threadActor;
    if (threadActor.state == "running") {
      threadActor.dbg.enabled = true;
    }

    this.emit("tabNavigated", {
      url: this.url,
      title: this.title,
      nativeConsoleAPI: this.hasNativeConsoleAPI(this.window),
      state: "stop",
      isFrameSwitching: isFrameSwitching
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
    assert(!this.exited, "Target must not be exited to create a sheet actor.");
    if (this._styleSheetActors.has(styleSheet)) {
      return this._styleSheetActors.get(styleSheet);
    }
    const actor = new StyleSheetActor(styleSheet, this);
    this._styleSheetActors.set(styleSheet, actor);

    this._targetScopedActorPool.addActor(actor);
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
exports.BrowsingContextTargetActor =
  ActorClassWithSpec(browsingContextTargetSpec, browsingContextTargetPrototype);

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
    Ci.nsIWebProgressListener,
    Ci.nsISupportsWeakReference,
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
    const docShellWindow = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                                 .getInterface(Ci.nsIDOMWindow);
    this._watchedDocShells.add(docShellWindow);

    const webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(this,
                                    Ci.nsIWebProgress.NOTIFY_STATE_WINDOW |
                                    Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT);

    const handler = getDocShellChromeEventHandler(docShell);
    handler.addEventListener("DOMWindowCreated", this._onWindowCreated, true);
    handler.addEventListener("pageshow", this._onWindowCreated, true);
    handler.addEventListener("pagehide", this._onWindowHidden, true);

    // Dispatch the _windowReady event on the targetActor for pre-existing windows
    for (const win of this._getWindowsInDocShell(docShell)) {
      this._targetActor._windowReady(win);
      this._knownWindowIDs.set(getWindowID(win), win);
    }
  },

  unwatch(docShell) {
    const docShellWindow = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                                 .getInterface(Ci.nsIDOMWindow);
    if (!this._watchedDocShells.has(docShellWindow)) {
      return;
    }

    const webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebProgress);
    // During process shutdown, the docshell may already be cleaned up and throw
    try {
      webProgress.removeProgressListener(this);
    } catch (e) {
      // ignore
    }

    const handler = getDocShellChromeEventHandler(docShell);
    handler.removeEventListener("DOMWindowCreated",
      this._onWindowCreated, true);
    handler.removeEventListener("pageshow", this._onWindowCreated, true);
    handler.removeEventListener("pagehide", this._onWindowHidden, true);

    for (const win of this._getWindowsInDocShell(docShell)) {
      this._knownWindowIDs.delete(getWindowID(win));
    }
  },

  _getWindowsInDocShell(docShell) {
    return getChildDocShells(docShell).map(d => {
      return d.QueryInterface(Ci.nsIInterfaceRequestor)
              .getInterface(Ci.nsIDOMWindow);
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
    const innerID = getWindowID(window);

    // This method is alled on DOMWindowCreated and pageshow
    // The common scenario is DOMWindowCreated, which is fired when the document
    // loads. But we are to listen for pageshow in order to handle BFCache.
    // When a page does into the BFCache, a pagehide event is fired with persisted=true
    // but it doesn't necessarely mean persisted will be true for the pageshow
    // event fired when the page is reloaded from the BFCache (see bug 1378133)
    // So just check if we already know this window and accept any that isn't known yet
    if (this._knownWindowIDs.has(innerID)) {
      return;
    }

    this._targetActor._windowReady(window);

    this._knownWindowIDs.set(innerID, window);
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
  }, "DebuggerProgressListener.prototype.observe"),

  onStateChange:
  DevToolsUtils.makeInfallible(function(progress, request, flag, status) {
    if (!this._targetActor.attached) {
      return;
    }

    const isStart = flag & Ci.nsIWebProgressListener.STATE_START;
    const isStop = flag & Ci.nsIWebProgressListener.STATE_STOP;
    const isDocument = flag & Ci.nsIWebProgressListener.STATE_IS_DOCUMENT;
    const isWindow = flag & Ci.nsIWebProgressListener.STATE_IS_WINDOW;

    // Catch any iframe location change
    if (isDocument && isStop) {
      // Watch document stop to ensure having the new iframe url.
      progress.QueryInterface(Ci.nsIDocShell);
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
      if (request.status != Cr.NS_OK && request.status != Cr.NS_BINDING_ABORTED) {
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
  }, "DebuggerProgressListener.prototype.onStateChange")
};
