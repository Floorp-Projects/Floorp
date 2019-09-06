/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu } = require("chrome");
const Services = require("Services");
const { Pool } = require("devtools/shared/protocol");
const {
  LazyPool,
  createExtraActors,
} = require("devtools/shared/protocol/lazy-pool");
const { DebuggerServer } = require("devtools/server/debugger-server");

loader.lazyRequireGetter(
  this,
  "ChromeWindowTargetActor",
  "devtools/server/actors/targets/chrome-window",
  true
);
loader.lazyRequireGetter(
  this,
  "ProcessDescriptorActor",
  "devtools/server/actors/descriptors/process",
  true
);

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
 *     - tabList: a live list (see below) of target actors for tabs. If present,
 *       the new root actor supports the 'listTabs' request, providing the live
 *       list's elements as its target actors, and sending 'tabListChanged'
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
 *       ActorRegistry.addGlobalActor. For each own property |P| of |A|,
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
  this._onServiceWorkerRegistrationListChanged = this.onServiceWorkerRegistrationListChanged.bind(
    this
  );
  this._onProcessListChanged = this.onProcessListChanged.bind(this);
  this._extraActors = {};

  this._globalActorPool = new LazyPool(this.conn);

  this._parentProcessTargetActor = null;
}

RootActor.prototype = {
  constructor: RootActor,
  applicationType: "browser",

  traits: {
    sources: true,
    networkMonitor: true,
    // Whether the storage inspector actor to inspect cookies, etc.
    storageInspector: true,
    // Whether storage inspector is read only
    storageInspectorReadOnly: true,
    // Whether the server can return wasm binary source
    wasmBinarySource: true,
    bulk: true,
    // Whether the director scripts are supported
    directorScripts: true,
    // Whether the debugger server supports
    // blackboxing (not supported in Fever Dream yet)
    noBlackBoxing: false,
    // Support for server pretty-printing has been removed.
    noPrettyPrinting: true,
    // Added in Firefox 66. Indicates that clients do not need to pause the
    // debuggee before adding breakpoints.
    breakpointWhileRunning: true,
    // Trait added in Gecko 38, indicating that all features necessary for
    // grabbing allocations from the MemoryActor are available for the performance tool
    memoryActorAllocations: true,
    // Added in Firefox 40. Indicates that the backend supports registering custom
    // commands through the WebConsoleCommands API.
    webConsoleCommands: true,
    // Whether root actor exposes chrome target actors and access to any window.
    // If allowChromeProcess is true, you can:
    // * get a ParentProcessTargetActor instance to debug chrome and any non-content
    //   resource via getProcess requests
    // * get a ChromeWindowTargetActor instance to debug windows which could be chrome,
    //   like browser windows via getWindow requests
    // If allowChromeProcess is defined, but not true, it means that root actor
    // no longer expose chrome target actors, but also that the above requests are
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
    // to retrieve the extension child process target actors.
    webExtensionAddonConnect: true,
    // Version of perf actor. Fx65+
    // Version 1 - Firefox 65: Introduces a duration-based buffer. It can be controlled
    // by adding a `duration` property (in seconds) to the options passed to
    // `front.startProfiler`. This is an optional parameter but it will throw an error if
    // the profiled Firefox doesn't accept it.
    perfActorVersion: 1,
    // Supports native log points and modifying the condition/log of an existing
    // breakpoints. Fx66+
    nativeLogpoints: true,
    // support older browsers for Fx69+
    hasThreadFront: true,
    // Support watchpoints in the server for Fx71+
    watchpoints: true,
  },

  /**
   * Return a 'hello' packet as specified by the Remote Debugging Protocol.
   */
  sayHello: function() {
    return {
      from: this.actorID,
      applicationType: this.applicationType,
      /* This is not in the spec, but it's used by tests. */
      testConnectionPrefix: this.conn.prefix,
      traits: this.traits,
    };
  },

  forwardingCancelled: function(prefix) {
    return {
      from: this.actorID,
      type: "forwardingCancelled",
      prefix,
    };
  },

  /**
   * Destroys the actor from the browser window.
   */
  destroy: function() {
    /* Tell the live lists we aren't watching any more. */
    if (this._parameters.tabList) {
      this._parameters.tabList.destroy();
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
    // Cleanup Actors on destroy
    if (this._tabTargetActorPool) {
      this._tabTargetActorPool.destroy();
    }
    if (this._processDescriptorActorPool) {
      this._processDescriptorActorPool.destroy();
    }
    if (this._globalActorPool) {
      this._globalActorPool.destroy();
    }
    if (this._chromeWindowActorPool) {
      this._chromeWindowActorPool.destroy();
    }
    if (this._addonTargetActorPool) {
      this._addonTargetActorPool.destroy();
    }
    if (this._workerTargetActorPool) {
      this._workerTargetActorPool.destroy();
    }
    if (this._serviceWorkerRegistrationActorPool) {
      this._serviceWorkerRegistrationActorPool.destroy();
    }
    this._extraActors = null;
    this.conn = null;
    this._tabTargetActorPool = null;
    this._globalActorPool = null;
    this._chromeWindowActorPool = null;
    this._parameters = null;
    this._parentProcessTargetActor = null;
  },

  /**
   * Gets the "root" form, which lists all the global actors that affect the entire
   * browser.  This can replace usages of `listTabs` that only wanted the global actors
   * and didn't actually care about tabs.
   */
  onGetRoot: function() {
    // Create global actors
    if (!this._globalActorPool) {
      this._globalActorPool = new LazyPool(this.conn);
    }
    const actors = createExtraActors(
      this._parameters.globalActorFactories,
      this._globalActorPool,
      this
    );

    return actors;
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
  onListTabs: async function(options) {
    const tabList = this._parameters.tabList;
    if (!tabList) {
      return {
        from: this.actorID,
        error: "noTabs",
        message: "This root actor has no browser tabs.",
      };
    }

    // Now that a client has requested the list of tabs, we reattach the onListChanged
    // listener in order to be notified if the list of tabs changes again in the future.
    tabList.onListChanged = this._onTabListChanged;

    // Walk the tab list, accumulating the array of target actors for the reply, and
    // moving all the actors to a new Pool. We'll replace the old tab target actor
    // pool with the one we build here, thus retiring any actors that didn't get listed
    // again, and preparing any new actors to receive packets.
    const newActorPool = new Pool(this.conn);
    const targetActorList = [];
    let selected;

    const targetActors = await tabList.getList(options);
    for (const targetActor of targetActors) {
      if (targetActor.exited) {
        // Target actor may have exited while we were gathering the list.
        continue;
      }
      if (targetActor.selected) {
        selected = targetActorList.length;
      }
      targetActor.parentID = this.actorID;
      newActorPool.manage(targetActor);
      targetActorList.push(targetActor);
    }

    // Start with the root reply, which includes the global actors for the whole browser.
    const reply = this.onGetRoot();

    // Drop the old actorID -> actor map. Actors that still mattered were added to the
    // new map; others will go away.
    if (this._tabTargetActorPool) {
      this._tabTargetActorPool.destroy();
    }
    this._tabTargetActorPool = newActorPool;

    // We'll extend the reply here to also mention all the tabs.
    Object.assign(reply, {
      selected: selected || 0,
      tabs: targetActorList.map(actor => actor.form()),
    });

    return reply;
  },

  onGetTab: async function(options) {
    const tabList = this._parameters.tabList;
    if (!tabList) {
      return {
        error: "noTabs",
        message: "This root actor has no browser tabs.",
      };
    }
    if (!this._tabTargetActorPool) {
      this._tabTargetActorPool = new Pool(this.conn);
    }

    let targetActor;
    try {
      targetActor = await tabList.getTab(options, { forceUnzombify: true });
    } catch (error) {
      if (error.error) {
        // Pipe expected errors as-is to the client
        return error;
      }
      return {
        error: "noTab",
        message: "Unexpected error while calling getTab(): " + error,
      };
    }

    targetActor.parentID = this.actorID;
    this._tabTargetActorPool.manage(targetActor);

    return { tab: targetActor.form() };
  },

  onGetWindow: function({ outerWindowID }) {
    if (!DebuggerServer.allowChromeProcess) {
      return {
        from: this.actorID,
        error: "forbidden",
        message: "You are not allowed to debug windows.",
      };
    }
    const window = Services.wm.getOuterWindowWithId(outerWindowID);
    if (!window) {
      return {
        from: this.actorID,
        error: "notFound",
        message: `No window found with outerWindowID ${outerWindowID}`,
      };
    }

    if (!this._chromeWindowActorPool) {
      this._chromeWindowActorPool = new Pool(this.conn);
    }

    const actor = new ChromeWindowTargetActor(this.conn, window);
    actor.parentID = this.actorID;
    this._chromeWindowActorPool.manage(actor);

    return {
      from: this.actorID,
      window: actor.form(),
    };
  },

  onTabListChanged: function() {
    this.conn.send({ from: this.actorID, type: "tabListChanged" });
    /* It's a one-shot notification; no need to watch any more. */
    this._parameters.tabList.onListChanged = null;
  },

  /**
   * This function can receive the following option from debugger client.
   *
   * @param {Object} option
   *        - iconDataURL: {boolean}
   *            When true, make data url from the icon of addon, then make possible to
   *            access by iconDataURL in the actor. The iconDataURL is useful when
   *            retrieving addons from a remote device, because the raw iconURL might not
   *            be accessible on the client.
   */
  onListAddons: async function(option) {
    const addonList = this._parameters.addonList;
    if (!addonList) {
      return {
        from: this.actorID,
        error: "noAddons",
        message: "This root actor has no browser addons.",
      };
    }

    // Reattach the onListChanged listener now that a client requested the list.
    addonList.onListChanged = this._onAddonListChanged;

    const addonTargetActors = await addonList.getList();
    const addonTargetActorPool = new Pool(this.conn);
    for (const addonTargetActor of addonTargetActors) {
      if (option.iconDataURL) {
        await addonTargetActor.loadIconDataURL();
      }

      addonTargetActorPool.manage(addonTargetActor);
    }

    if (this._addonTargetActorPool) {
      this._addonTargetActorPool.destroy();
    }
    this._addonTargetActorPool = addonTargetActorPool;

    return {
      from: this.actorID,
      addons: addonTargetActors.map(addonTargetActor =>
        addonTargetActor.form()
      ),
    };
  },

  onAddonListChanged: function() {
    this.conn.send({ from: this.actorID, type: "addonListChanged" });
    this._parameters.addonList.onListChanged = null;
  },

  onListWorkers: function() {
    const workerList = this._parameters.workerList;
    if (!workerList) {
      return {
        from: this.actorID,
        error: "noWorkers",
        message: "This root actor has no workers.",
      };
    }

    // Reattach the onListChanged listener now that a client requested the list.
    workerList.onListChanged = this._onWorkerListChanged;

    return workerList.getList().then(actors => {
      const pool = new Pool(this.conn);
      for (const actor of actors) {
        pool.manage(actor);
      }

      // Do not destroy the pool before transfering ownership to the newly created
      // pool, so that we do not accidently destroy actors that are still in use.
      if (this._workerTargetActorPool) {
        this._workerTargetActorPool.destroy();
      }

      this._workerTargetActorPool = pool;

      return {
        from: this.actorID,
        workers: actors.map(actor => actor.form()),
      };
    });
  },

  onWorkerListChanged: function() {
    this.conn.send({ from: this.actorID, type: "workerListChanged" });
    this._parameters.workerList.onListChanged = null;
  },

  onListServiceWorkerRegistrations: function() {
    const registrationList = this._parameters.serviceWorkerRegistrationList;
    if (!registrationList) {
      return {
        from: this.actorID,
        error: "noServiceWorkerRegistrations",
        message: "This root actor has no service worker registrations.",
      };
    }

    // Reattach the onListChanged listener now that a client requested the list.
    registrationList.onListChanged = this._onServiceWorkerRegistrationListChanged;

    return registrationList.getList().then(actors => {
      const pool = new Pool(this.conn);
      for (const actor of actors) {
        pool.manage(actor);
      }

      if (this._serviceWorkerRegistrationActorPool) {
        this._serviceWorkerRegistrationActorPool.destroy();
      }
      this._serviceWorkerRegistrationActorPool = pool;

      return {
        from: this.actorID,
        registrations: actors.map(actor => actor.form()),
      };
    });
  },

  onServiceWorkerRegistrationListChanged: function() {
    this.conn.send({
      from: this.actorID,
      type: "serviceWorkerRegistrationListChanged",
    });
    this._parameters.serviceWorkerRegistrationList.onListChanged = null;
  },

  onListProcesses: function() {
    const { processList } = this._parameters;
    if (!processList) {
      return {
        from: this.actorID,
        error: "noProcesses",
        message: "This root actor has no processes.",
      };
    }
    processList.onListChanged = this._onProcessListChanged;
    const processes = processList.getList();
    const pool = new Pool(this.conn);
    for (const metadata of processes) {
      let processDescriptor = this._getKnownProcessDescriptor(metadata.id);
      if (!processDescriptor) {
        processDescriptor = new ProcessDescriptorActor(this.conn, metadata);
      }
      pool.manage(processDescriptor);
    }
    // Do not destroy the pool before transfering ownership to the newly created
    // pool, so that we do not accidently destroy actors that are still in use.
    if (this._processDescriptorActorPool) {
      this._processDescriptorActorPool.destroy();
    }
    this._processDescriptorActorPool = pool;
    // extract the values in the processActors map
    const processActors = [...this._processDescriptorActorPool.poolChildren()];
    return {
      processes: processActors.map(actor => actor.form()),
    };
  },

  onProcessListChanged: function() {
    this.conn.send({ from: this.actorID, type: "processListChanged" });
    this._parameters.processList.onListChanged = null;
  },

  async onGetProcess(request) {
    if (!DebuggerServer.allowChromeProcess) {
      return {
        error: "forbidden",
        message: "You are not allowed to debug chrome.",
      };
    }
    if ("id" in request && typeof request.id != "number") {
      return {
        error: "wrongParameter",
        message: "getProcess requires a valid `id` attribute.",
      };
    }
    // If the request doesn't contains id parameter or id is 0
    // (id == 0, based on onListProcesses implementation)
    const id = request.id || 0;

    const mm = Services.ppmm.getChildAt(id);
    if (!mm) {
      return {
        error: "noProcess",
        message: "There is no process with id '" + id + "'.",
      };
    }
    let processDescriptor = this._getKnownProcessDescriptor(id);
    this._processDescriptorActorPool =
      this._processDescriptorActorPool || new Pool(this.conn);
    if (!processDescriptor) {
      const options = { id, parent: id === 0 };
      processDescriptor = new ProcessDescriptorActor(this.conn, options);
      this._processDescriptorActorPool.manage(processDescriptor);
    }
    return { form: processDescriptor.form() };
  },

  _getKnownProcessDescriptor(id) {
    // if there is no pool, then we do not have any descriptors
    if (!this._processDescriptorActorPool) {
      return null;
    }
    for (const descriptor of this._processDescriptorActorPool.poolChildren()) {
      if (descriptor.id === id) {
        return descriptor;
      }
    }
    return null;
  },

  /* This is not in the spec, but it's used by tests. */
  onEcho: function(request) {
    /*
     * Request packets are frozen. Copy request, so that
     * DebuggerServerConnection.onPacket can attach a 'from' property.
     */
    return Cu.cloneInto(request, {});
  },

  onProtocolDescription: function() {
    return require("devtools/shared/protocol").dumpProtocolSpec();
  },

  /**
   * Remove the extra actor (added by ActorRegistry.addGlobalActor or
   * ActorRegistry.addTargetScopedActor) name |name|.
   */
  removeActorByName: function(name) {
    if (name in this._extraActors) {
      const actor = this._extraActors[name];
      if (this._globalActorPool.has(actor.actorID)) {
        actor.destroy();
      }
      if (this._tabTargetActorPool) {
        // Iterate over BrowsingContextTargetActor instances to also remove target-scoped
        // actors created during listTabs for each document.
        for (const tab in this._tabTargetActorPool.poolChildren()) {
          tab.removeActorByName(name);
        }
      }
      delete this._extraActors[name];
    }
  },
};

RootActor.prototype.requestTypes = {
  getRoot: RootActor.prototype.onGetRoot,
  listTabs: RootActor.prototype.onListTabs,
  getTab: RootActor.prototype.onGetTab,
  getWindow: RootActor.prototype.onGetWindow,
  listAddons: RootActor.prototype.onListAddons,
  listWorkers: RootActor.prototype.onListWorkers,
  listServiceWorkerRegistrations:
    RootActor.prototype.onListServiceWorkerRegistrations,
  listProcesses: RootActor.prototype.onListProcesses,
  getProcess: RootActor.prototype.onGetProcess,
  echo: RootActor.prototype.onEcho,
  protocolDescription: RootActor.prototype.onProtocolDescription,
};

exports.RootActor = RootActor;
