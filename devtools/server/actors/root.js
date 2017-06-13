/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci, Cu } = require("chrome");
const Services = require("Services");
const { ActorPool, appendExtraActors, createExtraActors } = require("devtools/server/actors/common");
const { DebuggerServer } = require("devtools/server/main");

loader.lazyGetter(this, "ppmm", () => {
  return Cc["@mozilla.org/parentprocessmessagemanager;1"].getService(
    Ci.nsIMessageBroadcaster);
});
loader.lazyRequireGetter(this, "WindowActor",
  "devtools/server/actors/window", true);

/* Root actor for the remote debugging protocol. */

/**
 * Create a remote debugging protocol root actor.
 *
 * @param connection
 *     The DebuggerServerConnection whose root actor we are constructing.
 *
 * @param parameters
 *     The properties of |parameters| provide backing objects for the root
 *     actor's requests; if a given property is omitted from |parameters|, the
 *     root actor won't implement the corresponding requests or notifications.
 *     Supported properties:
 *
 *     - tabList: a live list (see below) of tab actors. If present, the
 *       new root actor supports the 'listTabs' request, providing the live
 *       list's elements as its tab actors, and sending 'tabListChanged'
 *       notifications when the live list's contents change. One actor in
 *       this list must have a true '.selected' property.
 *
 *     - addonList: a live list (see below) of addon actors. If present, the
 *       new root actor supports the 'listAddons' request, providing the live
 *       list's elements as its addon actors, and sending 'addonListchanged'
 *       notifications when the live list's contents change.
 *
 *     - globalActorFactories: an object |A| describing further actors to
 *       attach to the 'listTabs' reply. This is the type accumulated by
 *       DebuggerServer.addGlobalActor. For each own property |P| of |A|,
 *       the root actor adds a property named |P| to the 'listTabs'
 *       reply whose value is the name of an actor constructed by
 *       |A[P]|.
 *
 *     - onShutdown: a function to call when the root actor is destroyed.
 *
 * Instance properties:
 *
 * - applicationType: the string the root actor will include as the
 *      "applicationType" property in the greeting packet. By default, this
 *      is "browser".
 *
 * Live lists:
 *
 * A "live list", as used for the |tabList|, is an object that presents a
 * list of actors, and also notifies its clients of changes to the list. A
 * live list's interface is two properties:
 *
 * - getList: a method that returns a promise to the contents of the list.
 *
 * - onListChanged: a handler called, with no arguments, when the set of
 *             values the iterator would produce has changed since the last
 *             time 'iterator' was called. This may only be set to null or a
 *             callable value (one for which the typeof operator returns
 *             'function'). (Note that the live list will not call the
 *             onListChanged handler until the list has been iterated over
 *             once; if nobody's seen the list in the first place, nobody
 *             should care if its contents have changed!)
 *
 * When the list changes, the list implementation should ensure that any
 * actors yielded in previous iterations whose referents (tabs) still exist
 * get yielded again in subsequent iterations. If the underlying referent
 * is the same, the same actor should be presented for it.
 *
 * The root actor registers an 'onListChanged' handler on the appropriate
 * list when it may need to send the client 'tabListChanged' notifications,
 * and is careful to remove the handler whenever it does not need to send
 * such notifications (including when it is destroyed). This means that
 * live list implementations can use the state of the handler property (set
 * or null) to install and remove observers and event listeners.
 *
 * Note that, as the only way for the root actor to see the members of the
 * live list is to begin an iteration over the list, the live list need not
 * actually produce any actors until they are reached in the course of
 * iteration: alliterative lazy live lists.
 */
function RootActor(connection, parameters) {
  this.conn = connection;
  this._parameters = parameters;
  this._onTabListChanged = this.onTabListChanged.bind(this);
  this._onAddonListChanged = this.onAddonListChanged.bind(this);
  this._onWorkerListChanged = this.onWorkerListChanged.bind(this);
  this._onServiceWorkerRegistrationListChanged =
    this.onServiceWorkerRegistrationListChanged.bind(this);
  this._onProcessListChanged = this.onProcessListChanged.bind(this);
  this._extraActors = {};

  this._globalActorPool = new ActorPool(this.conn);
  this.conn.addActorPool(this._globalActorPool);

  this._chromeActor = null;
  this._processActors = new Map();
}

RootActor.prototype = {
  constructor: RootActor,
  applicationType: "browser",

  traits: {
    sources: true,
    // Whether the inspector actor allows modifying outer HTML.
    editOuterHTML: true,
    // Whether the inspector actor allows modifying innerHTML and inserting
    // adjacent HTML.
    pasteHTML: true,
    // Whether the server-side highlighter actor exists and can be used to
    // remotely highlight nodes (see server/actors/highlighters.js)
    highlightable: true,
    // Which custom highlighter does the server-side highlighter actor supports?
    // (see server/actors/highlighters.js)
    customHighlighters: true,
    // Whether the inspector actor implements the getImageDataFromURL
    // method that returns data-uris for image URLs. This is used for image
    // tooltips for instance
    urlToImageDataResolver: true,
    networkMonitor: true,
    // Whether the storage inspector actor to inspect cookies, etc.
    storageInspector: true,
    // Whether storage inspector is read only
    storageInspectorReadOnly: true,
    // Whether conditional breakpoints are supported
    conditionalBreakpoints: true,
    // Whether the server supports full source actors (breakpoints on
    // eval scripts, etc)
    debuggerSourceActors: true,
    bulk: true,
    // Whether the style rule actor implements the modifySelector method
    // that modifies the rule's selector
    selectorEditable: true,
    // Whether the page style actor implements the addNewRule method that
    // adds new rules to the page
    addNewRule: true,
    // Whether the dom node actor implements the getUniqueSelector method
    getUniqueSelector: true,
    // Whether the dom node actor implements the getCssPath method
    getCssPath: true,
    // Whether the dom node actor implements the getXPath method
    getXPath: true,
    // Whether the director scripts are supported
    directorScripts: true,
    // Whether the debugger server supports
    // blackboxing/pretty-printing (not supported in Fever Dream yet)
    noBlackBoxing: false,
    noPrettyPrinting: false,
    // Whether the page style actor implements the getUsedFontFaces method
    // that returns the font faces used on a node
    getUsedFontFaces: true,
    // Trait added in Gecko 38, indicating that all features necessary for
    // grabbing allocations from the MemoryActor are available for the performance tool
    memoryActorAllocations: true,
    // Added in Gecko 40, indicating that the backend isn't stupid about
    // sending resumption packets on tab navigation.
    noNeedToFakeResumptionOnNavigation: true,
    // Added in Firefox 40. Indicates that the backend supports registering custom
    // commands through the WebConsoleCommands API.
    webConsoleCommands: true,
    // Whether root actor exposes tab actors and access to any window.
    // If allowChromeProcess is true, you can:
    // * get a ChromeActor instance to debug chrome and any non-content
    //   resource via getProcess requests
    // * get a WindowActor instance to debug windows which could be chrome,
    //   like browser windows via getWindow requests
    // If allowChromeProcess is defined, but not true, it means that root actor
    // no longer expose tab actors, but also that the above requests are
    // forbidden for security reasons.
    get allowChromeProcess() {
      return DebuggerServer.allowChromeProcess;
    },
    // Whether or not `getProfile()` supports specifying a `startTime`
    // and `endTime` to filter out samples. Fx40+
    profilerDataFilterable: true,
    // Whether or not the MemoryActor's heap snapshot abilities are
    // fully equipped to handle heap snapshots for the memory tool. Fx44+
    heapSnapshots: true,
    // Whether or not the timeline actor can emit DOMContentLoaded and Load
    // markers, currently in use by the network monitor. Fx45+
    documentLoadingMarkers: true,
    // Whether or not the webextension addon actor have to be connected
    // to retrieve the extension child process tab actors.
    webExtensionAddonConnect: true,
  },

  /**
   * Return a 'hello' packet as specified by the Remote Debugging Protocol.
   */
  sayHello: function () {
    return {
      from: this.actorID,
      applicationType: this.applicationType,
      /* This is not in the spec, but it's used by tests. */
      testConnectionPrefix: this.conn.prefix,
      traits: this.traits
    };
  },

  forwardingCancelled: function (prefix) {
    return {
      from: this.actorID,
      type: "forwardingCancelled",
      prefix,
    };
  },

  /**
   * Destroys the actor from the browser window.
   */
  destroy: function () {
    /* Tell the live lists we aren't watching any more. */
    if (this._parameters.tabList) {
      this._parameters.tabList.onListChanged = null;
    }
    if (this._parameters.addonList) {
      this._parameters.addonList.onListChanged = null;
    }
    if (this._parameters.workerList) {
      this._parameters.workerList.onListChanged = null;
    }
    if (this._parameters.serviceWorkerRegistrationList) {
      this._parameters.serviceWorkerRegistrationList.onListChanged = null;
    }
    if (this._parameters.processList) {
      this._parameters.processList.onListChanged = null;
    }
    if (typeof this._parameters.onShutdown === "function") {
      this._parameters.onShutdown();
    }
    this._extraActors = null;
    this.conn = null;
    this._tabActorPool = null;
    this._globalActorPool = null;
    this._windowActorPool = null;
    this._parameters = null;
    this._chromeActor = null;
    this._processActors.clear();
  },

  /**
   * Gets the "root" form, which lists all the global actors that affect the entire
   * browser.  This can replace usages of `listTabs` that only wanted the global actors
   * and didn't actually care about tabs.
   */
  onGetRoot: function () {
    let reply = {
      from: this.actorID,
    };

    // Create global actors
    if (!this._globalActorPool) {
      this._globalActorPool = new ActorPool(this.conn);
      this.conn.addActorPool(this._globalActorPool);
    }
    this._createExtraActors(this._parameters.globalActorFactories, this._globalActorPool);

    // List the global actors
    this._appendExtraActors(reply);

    return reply;
  },

  /* The 'listTabs' request and the 'tabListChanged' notification. */

  /**
   * Handles the listTabs request. The actors will survive until at least
   * the next listTabs request.
   *
   * ⚠ WARNING ⚠ This can be a very expensive operation, especially if there are many
   * open tabs.  It will cause us to visit every tab, load a frame script, start a
   * debugger server, and read some data.  With lazy tab support (bug 906076), this
   * would trigger any lazy tabs to be loaded, greatly increasing resource usage.  Avoid
   * this method whenever possible.
   */
  onListTabs: async function () {
    let tabList = this._parameters.tabList;
    if (!tabList) {
      return { from: this.actorID, error: "noTabs",
               message: "This root actor has no browser tabs." };
    }

    // Now that a client has requested the list of tabs, we reattach the onListChanged
    // listener in order to be notified if the list of tabs changes again in the future.
    tabList.onListChanged = this._onTabListChanged;

    // Walk the tab list, accumulating the array of tab actors for the reply, and moving
    // all the actors to a new ActorPool. We'll replace the old tab actor pool with the
    // one we build here, thus retiring any actors that didn't get listed again, and
    // preparing any new actors to receive packets.
    let newActorPool = new ActorPool(this.conn);
    let tabActorList = [];
    let selected;

    let tabActors = await tabList.getList();
    for (let tabActor of tabActors) {
      if (tabActor.exited) {
        // Tab actor may have exited while we were gathering the list.
        continue;
      }
      if (tabActor.selected) {
        selected = tabActorList.length;
      }
      tabActor.parentID = this.actorID;
      newActorPool.addActor(tabActor);
      tabActorList.push(tabActor);
    }

    // Start with the root reply, which includes the global actors for the whole browser.
    let reply = this.onGetRoot();

    // Drop the old actorID -> actor map. Actors that still mattered were added to the
    // new map; others will go away.
    if (this._tabActorPool) {
      this.conn.removeActorPool(this._tabActorPool);
    }
    this._tabActorPool = newActorPool;
    this.conn.addActorPool(this._tabActorPool);

    // We'll extend the reply here to also mention all the tabs.
    Object.assign(reply, {
      selected: selected || 0,
      tabs: tabActorList.map(actor => actor.form()),
    });

    return reply;
  },

  onGetTab: async function (options) {
    let tabList = this._parameters.tabList;
    if (!tabList) {
      return { error: "noTabs",
               message: "This root actor has no browser tabs." };
    }
    if (!this._tabActorPool) {
      this._tabActorPool = new ActorPool(this.conn);
      this.conn.addActorPool(this._tabActorPool);
    }

    let tabActor;
    try {
      tabActor = await tabList.getTab(options);
    } catch (error) {
      if (error.error) {
        // Pipe expected errors as-is to the client
        return error;
      }
      return {
        error: "noTab",
        message: "Unexpected error while calling getTab(): " + error
      };
    }

    tabActor.parentID = this.actorID;
    this._tabActorPool.addActor(tabActor);

    return { tab: tabActor.form() };
  },

  onGetWindow: function ({ outerWindowID }) {
    if (!DebuggerServer.allowChromeProcess) {
      return {
        from: this.actorID,
        error: "forbidden",
        message: "You are not allowed to debug windows."
      };
    }
    let window = Services.wm.getOuterWindowWithId(outerWindowID);
    if (!window) {
      return {
        from: this.actorID,
        error: "notFound",
        message: `No window found with outerWindowID ${outerWindowID}`,
      };
    }

    if (!this._windowActorPool) {
      this._windowActorPool = new ActorPool(this.conn);
      this.conn.addActorPool(this._windowActorPool);
    }

    let actor = new WindowActor(this.conn, window);
    actor.parentID = this.actorID;
    this._windowActorPool.addActor(actor);

    return {
      from: this.actorID,
      window: actor.form(),
    };
  },

  onTabListChanged: function () {
    this.conn.send({ from: this.actorID, type: "tabListChanged" });
    /* It's a one-shot notification; no need to watch any more. */
    this._parameters.tabList.onListChanged = null;
  },

  onListAddons: function () {
    let addonList = this._parameters.addonList;
    if (!addonList) {
      return { from: this.actorID, error: "noAddons",
               message: "This root actor has no browser addons." };
    }

    // Reattach the onListChanged listener now that a client requested the list.
    addonList.onListChanged = this._onAddonListChanged;

    return addonList.getList().then((addonActors) => {
      let addonActorPool = new ActorPool(this.conn);
      for (let addonActor of addonActors) {
        addonActorPool.addActor(addonActor);
      }

      if (this._addonActorPool) {
        this.conn.removeActorPool(this._addonActorPool);
      }
      this._addonActorPool = addonActorPool;
      this.conn.addActorPool(this._addonActorPool);

      return {
        "from": this.actorID,
        "addons": addonActors.map(addonActor => addonActor.form())
      };
    });
  },

  onAddonListChanged: function () {
    this.conn.send({ from: this.actorID, type: "addonListChanged" });
    this._parameters.addonList.onListChanged = null;
  },

  onListWorkers: function () {
    let workerList = this._parameters.workerList;
    if (!workerList) {
      return { from: this.actorID, error: "noWorkers",
               message: "This root actor has no workers." };
    }

    // Reattach the onListChanged listener now that a client requested the list.
    workerList.onListChanged = this._onWorkerListChanged;

    return workerList.getList().then(actors => {
      let pool = new ActorPool(this.conn);
      for (let actor of actors) {
        pool.addActor(actor);
      }

      this.conn.removeActorPool(this._workerActorPool);
      this._workerActorPool = pool;
      this.conn.addActorPool(this._workerActorPool);

      return {
        "from": this.actorID,
        "workers": actors.map(actor => actor.form())
      };
    });
  },

  onWorkerListChanged: function () {
    this.conn.send({ from: this.actorID, type: "workerListChanged" });
    this._parameters.workerList.onListChanged = null;
  },

  onListServiceWorkerRegistrations: function () {
    let registrationList = this._parameters.serviceWorkerRegistrationList;
    if (!registrationList) {
      return { from: this.actorID, error: "noServiceWorkerRegistrations",
               message: "This root actor has no service worker registrations." };
    }

    // Reattach the onListChanged listener now that a client requested the list.
    registrationList.onListChanged = this._onServiceWorkerRegistrationListChanged;

    return registrationList.getList().then(actors => {
      let pool = new ActorPool(this.conn);
      for (let actor of actors) {
        pool.addActor(actor);
      }

      this.conn.removeActorPool(this._serviceWorkerRegistrationActorPool);
      this._serviceWorkerRegistrationActorPool = pool;
      this.conn.addActorPool(this._serviceWorkerRegistrationActorPool);

      return {
        "from": this.actorID,
        "registrations": actors.map(actor => actor.form())
      };
    });
  },

  onServiceWorkerRegistrationListChanged: function () {
    this.conn.send({ from: this.actorID, type: "serviceWorkerRegistrationListChanged" });
    this._parameters.serviceWorkerRegistrationList.onListChanged = null;
  },

  onListProcesses: function () {
    let { processList } = this._parameters;
    if (!processList) {
      return { from: this.actorID, error: "noProcesses",
               message: "This root actor has no processes." };
    }
    processList.onListChanged = this._onProcessListChanged;
    return {
      processes: processList.getList()
    };
  },

  onProcessListChanged: function () {
    this.conn.send({ from: this.actorID, type: "processListChanged" });
    this._parameters.processList.onListChanged = null;
  },

  onGetProcess: function (request) {
    if (!DebuggerServer.allowChromeProcess) {
      return { error: "forbidden",
               message: "You are not allowed to debug chrome." };
    }
    if (("id" in request) && typeof (request.id) != "number") {
      return { error: "wrongParameter",
               message: "getProcess requires a valid `id` attribute." };
    }
    // If the request doesn't contains id parameter or id is 0
    // (id == 0, based on onListProcesses implementation)
    if ((!("id" in request)) || request.id === 0) {
      if (!this._chromeActor) {
        // Create a ChromeActor for the parent process
        let { ChromeActor } = require("devtools/server/actors/chrome");
        this._chromeActor = new ChromeActor(this.conn);
        this._globalActorPool.addActor(this._chromeActor);
      }

      return { form: this._chromeActor.form() };
    }

    let { id } = request;
    let mm = ppmm.getChildAt(id);
    if (!mm) {
      return { error: "noProcess",
               message: "There is no process with id '" + id + "'." };
    }
    let form = this._processActors.get(id);
    if (form) {
      return { form };
    }
    let onDestroy = () => {
      this._processActors.delete(id);
    };
    return DebuggerServer.connectToContent(this.conn, mm, onDestroy).then(formResult => {
      this._processActors.set(id, formResult);
      return { form: formResult };
    });
  },

  /* This is not in the spec, but it's used by tests. */
  onEcho: function (request) {
    /*
     * Request packets are frozen. Copy request, so that
     * DebuggerServerConnection.onPacket can attach a 'from' property.
     */
    return Cu.cloneInto(request, {});
  },

  onProtocolDescription: function () {
    return require("devtools/shared/protocol").dumpProtocolSpec();
  },

  /* Support for DebuggerServer.addGlobalActor. */
  _createExtraActors: createExtraActors,
  _appendExtraActors: appendExtraActors,

  /**
   * Remove the extra actor (added by DebuggerServer.addGlobalActor or
   * DebuggerServer.addTabActor) name |name|.
   */
  removeActorByName: function (name) {
    if (name in this._extraActors) {
      const actor = this._extraActors[name];
      if (this._globalActorPool.has(actor)) {
        this._globalActorPool.removeActor(actor);
      }
      if (this._tabActorPool) {
        // Iterate over TabActor instances to also remove tab actors
        // created during listTabs for each document.
        this._tabActorPool.forEach(tab => {
          tab.removeActorByName(name);
        });
      }
      delete this._extraActors[name];
    }
  }
};

RootActor.prototype.requestTypes = {
  getRoot: RootActor.prototype.onGetRoot,
  listTabs: RootActor.prototype.onListTabs,
  getTab: RootActor.prototype.onGetTab,
  getWindow: RootActor.prototype.onGetWindow,
  listAddons: RootActor.prototype.onListAddons,
  listWorkers: RootActor.prototype.onListWorkers,
  listServiceWorkerRegistrations: RootActor.prototype.onListServiceWorkerRegistrations,
  listProcesses: RootActor.prototype.onListProcesses,
  getProcess: RootActor.prototype.onGetProcess,
  echo: RootActor.prototype.onEcho,
  protocolDescription: RootActor.prototype.onProtocolDescription
};

exports.RootActor = RootActor;
