/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// protocol.js uses objects as exceptions in order to define
// error packets.
/* eslint-disable no-throw-literal */

const { Cu } = require("chrome");
const Services = require("Services");
const { Pool } = require("devtools/shared/protocol");
const {
  LazyPool,
  createExtraActors,
} = require("devtools/shared/protocol/lazy-pool");
const { DevToolsServer } = require("devtools/server/devtools-server");
const protocol = require("devtools/shared/protocol");
const { rootSpec } = require("devtools/shared/specs/root");

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
loader.lazyRequireGetter(
  this,
  "FrameDescriptorActor",
  "devtools/server/actors/descriptors/frame",
  true
);

/* Root actor for the remote debugging protocol. */

/**
 * Create a remote debugging protocol root actor.
 *
 * @param conn
 *     The DevToolsServerConnection whose root actor we are constructing.
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
exports.RootActor = protocol.ActorClassWithSpec(rootSpec, {
  initialize: function(conn, parameters) {
    protocol.Actor.prototype.initialize.call(this, conn);

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

    this.applicationType = "browser";

    this.traits = {
      sources: true,
      networkMonitor: true,
      // Whether the storage inspector actor to inspect cookies, etc.
      storageInspector: true,
      // Whether the server can return wasm binary source
      wasmBinarySource: true,
      bulk: true,
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
        return DevToolsServer.allowChromeProcess;
      },
      // Whether or not the MemoryActor's heap snapshot abilities are
      // fully equipped to handle heap snapshots for the memory tool. Fx44+
      heapSnapshots: true,
      // Version of perf actor. Fx65+
      // Version 1 - Firefox 65: Introduces a duration-based buffer. It can be controlled
      // by adding a `duration` property (in seconds) to the options passed to
      // `front.startProfiler`. This is an optional parameter but it will throw an error if
      // the profiled Firefox doesn't accept it.
      perfActorVersion: 1,
      // Supports watchpoints in the server for Fx71+
      watchpoints: true,
    };
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
    protocol.Actor.prototype.destroy.call(this);

    /* Tell the live lists we aren't watching any more. */
    if (this._parameters.tabList) {
      this._parameters.tabList.destroy();
    }
    if (this._parameters.addonList) {
      this._parameters.addonList.onListChanged = null;
    }
    if (this._parameters.workerList) {
      this._parameters.workerList.destroy();
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
    if (this._tabDescriptorActorPool) {
      this._tabDescriptorActorPool.destroy();
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
    if (this._frameDescriptorActorPool) {
      this._frameDescriptorActorPool.destroy();
    }

    if (this._serviceWorkerRegistrationActorPool) {
      this._serviceWorkerRegistrationActorPool.destroy();
    }
    this._extraActors = null;
    this.conn = null;
    this._tabDescriptorActorPool = null;
    this._globalActorPool = null;
    this._chromeWindowActorPool = null;
    this._parameters = null;
  },

  /**
   * Gets the "root" form, which lists all the global actors that affect the entire
   * browser.  This can replace usages of `listTabs` that only wanted the global actors
   * and didn't actually care about tabs.
   */
  getRoot: function() {
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
   * devtools server, and read some data.  With lazy tab support (bug 906076), this
   * would trigger any lazy tabs to be loaded, greatly increasing resource usage.  Avoid
   * this method whenever possible.
   */
  listTabs: async function() {
    const tabList = this._parameters.tabList;
    if (!tabList) {
      throw {
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
    const newActorPool = new Pool(this.conn, "listTabs-tab-descriptors");
    let selected;

    const tabDescriptorActors = await tabList.getList();
    for (const tabDescriptorActor of tabDescriptorActors) {
      if (tabDescriptorActor.selected) {
        const index = tabDescriptorActors.findIndex(
          descriptor => descriptor === tabDescriptorActor
        );
        selected = index;
      }
      newActorPool.manage(tabDescriptorActor);
    }

    // Start with the root reply, which includes the global actors for the whole browser.
    const reply = this.getRoot();

    // Drop the old actorID -> actor map. Actors that still mattered were added to the
    // new map; others will go away.
    if (this._tabDescriptorActorPool) {
      this._tabDescriptorActorPool.destroy();
    }
    this._tabDescriptorActorPool = newActorPool;

    // We'll extend the reply here to also mention all the tabs.
    Object.assign(reply, {
      selected: selected || 0,
      tabs: [...this._tabDescriptorActorPool.poolChildren()],
    });

    return reply;
  },

  getTab: async function({ outerWindowID, tabId }) {
    const tabList = this._parameters.tabList;
    if (!tabList) {
      throw {
        error: "noTabs",
        message: "This root actor has no browser tabs.",
      };
    }
    if (!this._tabDescriptorActorPool) {
      this._tabDescriptorActorPool = new Pool(
        this.conn,
        "getTab-tab-descriptors"
      );
    }

    let descriptorActor;
    try {
      descriptorActor = await tabList.getTab({ outerWindowID, tabId });
    } catch (error) {
      if (error.error) {
        // Pipe expected errors as-is to the client
        throw error;
      }
      throw {
        error: "noTab",
        message: "Unexpected error while calling getTab(): " + error,
      };
    }

    descriptorActor.parentID = this.actorID;
    this._tabDescriptorActorPool.manage(descriptorActor);

    return descriptorActor;
  },

  getWindow: function({ outerWindowID }) {
    if (!DevToolsServer.allowChromeProcess) {
      throw {
        error: "forbidden",
        message: "You are not allowed to debug windows.",
      };
    }
    const window = Services.wm.getOuterWindowWithId(outerWindowID);
    if (!window) {
      throw {
        error: "notFound",
        message: `No window found with outerWindowID ${outerWindowID}`,
      };
    }

    if (!this._chromeWindowActorPool) {
      this._chromeWindowActorPool = new Pool(this.conn, "chrome-window");
    }

    const actor = new ChromeWindowTargetActor(this.conn, window);
    actor.parentID = this.actorID;
    this._chromeWindowActorPool.manage(actor);

    return actor;
  },

  onTabListChanged: function() {
    this.conn.send({ from: this.actorID, type: "tabListChanged" });
    /* It's a one-shot notification; no need to watch any more. */
    this._parameters.tabList.onListChanged = null;
  },

  /**
   * This function can receive the following option from devtools client.
   *
   * @param {Object} option
   *        - iconDataURL: {boolean}
   *            When true, make data url from the icon of addon, then make possible to
   *            access by iconDataURL in the actor. The iconDataURL is useful when
   *            retrieving addons from a remote device, because the raw iconURL might not
   *            be accessible on the client.
   */
  listAddons: async function(option) {
    const addonList = this._parameters.addonList;
    if (!addonList) {
      throw {
        error: "noAddons",
        message: "This root actor has no browser addons.",
      };
    }

    // Reattach the onListChanged listener now that a client requested the list.
    addonList.onListChanged = this._onAddonListChanged;

    const addonTargetActors = await addonList.getList();
    const addonTargetActorPool = new Pool(this.conn, "addon-descriptors");
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

    return addonTargetActors;
  },

  onAddonListChanged: function() {
    this.conn.send({ from: this.actorID, type: "addonListChanged" });
    this._parameters.addonList.onListChanged = null;
  },

  listWorkers: function() {
    const workerList = this._parameters.workerList;
    if (!workerList) {
      throw {
        error: "noWorkers",
        message: "This root actor has no workers.",
      };
    }

    // Reattach the onListChanged listener now that a client requested the list.
    workerList.onListChanged = this._onWorkerListChanged;

    return workerList.getList().then(actors => {
      const pool = new Pool(this.conn, "worker-targets");
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
        workers: actors,
      };
    });
  },

  onWorkerListChanged: function() {
    this.conn.send({ from: this.actorID, type: "workerListChanged" });
    this._parameters.workerList.onListChanged = null;
  },

  listServiceWorkerRegistrations: function() {
    const registrationList = this._parameters.serviceWorkerRegistrationList;
    if (!registrationList) {
      throw {
        error: "noServiceWorkerRegistrations",
        message: "This root actor has no service worker registrations.",
      };
    }

    // Reattach the onListChanged listener now that a client requested the list.
    registrationList.onListChanged = this._onServiceWorkerRegistrationListChanged;

    return registrationList.getList().then(actors => {
      const pool = new Pool(this.conn, "service-workers-registrations");
      for (const actor of actors) {
        pool.manage(actor);
      }

      if (this._serviceWorkerRegistrationActorPool) {
        this._serviceWorkerRegistrationActorPool.destroy();
      }
      this._serviceWorkerRegistrationActorPool = pool;

      return {
        registrations: actors,
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

  listProcesses: function() {
    const { processList } = this._parameters;
    if (!processList) {
      throw {
        error: "noProcesses",
        message: "This root actor has no processes.",
      };
    }
    processList.onListChanged = this._onProcessListChanged;
    const processes = processList.getList();
    const pool = new Pool(this.conn, "process-descriptors");
    for (const metadata of processes) {
      let processDescriptor = this._getKnownDescriptor(
        metadata.id,
        this._processDescriptorActorPool
      );
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
    return [...this._processDescriptorActorPool.poolChildren()];
  },

  onProcessListChanged: function() {
    this.conn.send({ from: this.actorID, type: "processListChanged" });
    this._parameters.processList.onListChanged = null;
  },

  async getProcess(id) {
    if (!DevToolsServer.allowChromeProcess) {
      throw {
        error: "forbidden",
        message: "You are not allowed to debug chrome.",
      };
    }
    if (typeof id != "number") {
      throw {
        error: "wrongParameter",
        message: "getProcess requires a valid `id` attribute.",
      };
    }
    this._processDescriptorActorPool =
      this._processDescriptorActorPool ||
      new Pool(this.conn, "process-descriptors");

    let processDescriptor = this._getKnownDescriptor(
      id,
      this._processDescriptorActorPool
    );
    if (!processDescriptor) {
      // The parent process has id == 0, based on ProcessActorList::getList implementation
      const options = { id, parent: id === 0 };
      processDescriptor = new ProcessDescriptorActor(this.conn, options);
      this._processDescriptorActorPool.manage(processDescriptor);
    }
    return processDescriptor;
  },

  /**
   * Note that this method behaves differently when called for a top level
   * window.
   * For a top level window, it will ONLY return remote browser elements.
   * For any other window, it will return chidren elements, remote or not.
   *
   * Also important to note, this method only returns direct children, not the
   * complete browsing context tree.
   */
  async _getChildBrowsingContexts(id) {
    // If we have the id of the parent, then we need to get the child
    // contexts in a special way. We have a method on the descriptor
    // to take care of this.
    const parentBrowsingContext = BrowsingContext.get(id);
    // If this is a parent-process window, and it's top-level (not embedded in a browser),
    // collect all the remote browsers in the window ourselves. getChildren() will not return
    // these contexts otherwise.
    if (
      parentBrowsingContext.window &&
      !parentBrowsingContext.embedderElement
    ) {
      const { window } = parentBrowsingContext;
      return [
        ...window.document.querySelectorAll(`browser[remote="true"]`),
      ].map(browser => browser.browsingContext);
    }
    // for all other contexts, since we do not need to get contexts of
    // a different type, we can just get the children directly from
    // the BrowsingContext.
    return (
      parentBrowsingContext.children
        // For now, we only return the "remote frames".
        // i.e. the frames which are in a distinct process compared to their parent document
        .filter(browsingContext => {
          return (
            !browsingContext.parent ||
            browsingContext.currentWindowGlobal.osPid !=
              browsingContext.parent.currentWindowGlobal.osPid
          );
        })
    );
  },

  async listRemoteFrames(id) {
    const frames = [];
    const contextsToWalk = await this._getChildBrowsingContexts(id);

    if (contextsToWalk.length == 0) {
      return { frames };
    }

    const pool = new Pool(this.conn, "frame-descriptors");
    while (contextsToWalk.length) {
      const currentContext = contextsToWalk.pop();
      let frameDescriptor = this._getKnownDescriptor(
        currentContext.id,
        this._frameDescriptorActorPool
      );
      if (!frameDescriptor) {
        frameDescriptor = new FrameDescriptorActor(this.conn, currentContext);
      }
      pool.manage(frameDescriptor);
      frames.push(frameDescriptor);
      contextsToWalk.push(...currentContext.children);
    }
    // Do not destroy the pool before transfering ownership to the newly created
    // pool, so that we do not accidently destroy actors that are still in use.
    if (this._frameDescriptorActorPool) {
      this._frameDescriptorActorPool.destroy();
    }

    this._frameDescriptorActorPool = pool;

    return { frames };
  },

  _getKnownDescriptor(id, pool) {
    // if there is no pool, then we do not have any descriptors
    if (!pool) {
      return null;
    }
    for (const descriptor of pool.poolChildren()) {
      if (descriptor.id === id) {
        return descriptor;
      }
    }
    return null;
  },

  _getParentProcessDescriptor() {
    if (!this._processDescriptorActorPool) {
      this._processDescriptorActorPool = new Pool(
        this.conn,
        "process-descriptors"
      );
      const options = { id: 0, parent: true };
      const descriptor = new ProcessDescriptorActor(this.conn, options);
      this._processDescriptorActorPool.manage(descriptor);
      return descriptor;
    }
    for (const descriptor of this._processDescriptorActorPool.poolChildren()) {
      if (descriptor.isParent) {
        return descriptor;
      }
    }
    return null;
  },

  _isParentBrowsingContext(id) {
    // TODO: We may stop making the parent process codepath so special
    const window = Services.wm.getMostRecentWindow(
      DevToolsServer.chromeWindowType
    );
    return id == window.docShell.browsingContext.id;
  },

  getBrowsingContextDescriptor(id) {
    // since the id for frame descriptors is the same as the browsing
    // context id, we can get the associated descriptor using
    // _getKnownDescriptor.
    const frameDescriptor = this._getKnownDescriptor(
      id,
      this._frameDescriptorActorPool
    );
    if (frameDescriptor) {
      return frameDescriptor;
    }

    // if the descriptor cannot be found in the frames, it is probably
    // the main process, which is a process descriptor
    if (this._isParentBrowsingContext(id)) {
      return this._getParentProcessDescriptor();
    }

    const context = BrowsingContext.get(id);
    const newFrameDescriptor = new FrameDescriptorActor(this.conn, context);
    if (!this._frameDescriptorActorPool) {
      this._frameDescriptorActorPool = new Pool(this.conn, "frame-descriptors");
    }
    this._frameDescriptorActorPool.manage(newFrameDescriptor);
    return newFrameDescriptor;
  },

  protocolDescription: function() {
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
      if (this._tabDescriptorActorPool) {
        // Iterate over BrowsingContextTargetActor instances to also remove target-scoped
        // actors created during listTabs for each document.
        for (const tab in this._tabDescriptorActorPool.poolChildren()) {
          tab.removeActorByName(name);
        }
      }
      delete this._extraActors[name];
    }
  },
});

/**
 * This `echo` request can't be easily specified via protocol.js types
 * as it is a JSON value in the packet itself. Protocol.js only allows
 * arbitrary json object in one property of the packet.
 * In order to bypass protocol.js, declare the request method directly
 * on the prototype/requestTypes, which is populated by ActorClassWithSpec.
 *
 * Note that this request is only used by tests.
 */
exports.RootActor.prototype.requestTypes.echo = function(request) {
  /*
   * Request packets are frozen. Copy request, so that
   * DevToolsServerConnection.onPacket can attach a 'from' property.
   */
  return Cu.cloneInto(request, {});
};
